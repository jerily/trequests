
lappend auto_path [file join [file dirname [info script]] json]
package require json

tcltest::testConstraint testingModeEnabled [trequests::pkgconfig get testing-mode]
tcltest::testConstraint testingModeDisabled [expr { ![trequests::pkgconfig get testing-mode] }]

tcltest::testConstraint curlFeatureEnabledGSS [expr { [lsearch -exact [trequests::curl_version features] "GSS-API"] != -1 }]
tcltest::testConstraint curlFeatureDisabledGSS [expr { [lsearch -exact [trequests::curl_version features] "GSS-API"] == -1 }]

# Pre-create a default pool to avoid memory leaks, report memory regions
# associated with the async pool that was created during test execution
# and will be freed on exit.

catch {
    set r [::trequests::get http://localhost -async]
    $r destroy
    unset r
}

proc define_constants { data } {

    foreach line [split $data \n] {
        if { [set line [string trim $line]] eq "" || [string index $line 0] eq "#" } continue
        set line [split $line]
        set var [lindex $line 0]
        set expr [join [lrange $line 1 end] " "]
        set val [uplevel #0 [list expr $expr]]
        set ::$var $val
    }

}

proc dict2output { data { level 0 } } {
    set result [list]
    foreach k [lsort [dict keys $data]] {
        set v [dict get $data $k]
        if { $k in {X-Amzn-Trace-Id origin} } continue
        if { $k in {headers} } {
            set v "\n[dict2output $v [expr { $level + 1 }]]\n"
        }
        lappend result [string repeat {  } $level ][list $k $v]
    }
    return [join $result \n]
}

proc httpbin { code req } {
    set body [$req text]
    if { $code != [$req status_code] } {
        return "ERROR: bad status code: [$req status_code] != $code\nBODY: $body"
    }
    if { [string trim $body] eq "" } {
        return ""
    }
    return [dict2output [::json::json2dict $body]]
}


