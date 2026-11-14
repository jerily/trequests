# /**
#  * Copyright Jerily LTD. All Rights Reserved.
#  * SPDX-FileCopyrightText: 2026 Neofytos Dimitriou (neo@jerily.cy)
#  * SPDX-License-Identifier: MIT.
#  */

# Separate process so synchronous libcurl requests cannot block the server loop.
proc accept {channel address port} {
    chan configure $channel -blocking 0 -translation binary
    set ::input($channel) {}
    fileevent $channel readable [list receive $channel]
}
proc receive {channel} {
    append ::input($channel) [read $channel]
    if {[eof $channel]} {
        close $channel
        unset ::input($channel)
        return
    }
    set end [string first \r\n\r\n $::input($channel)]
    if {$end < 0} {return}
    set headers [string range $::input($channel) 0 [expr {$end - 1}]]
    set path [lindex [split [lindex [split $headers "\n"] 0]] 1]
    if {$path eq "/gate"} {
        fileevent $channel readable {}
        puts -nonewline $channel "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Type: application/octet-stream\r\n\r\n3\r\none\r\n"
        flush $channel
        lappend ::gates $channel
        unset ::input($channel)
        return
    }
    if {$path eq "/release"} {
        foreach gate $::gates {
            catch {puts -nonewline $gate "3\r\ntwo\r\n0\r\n\r\n"; close $gate}
        }
        set ::gates {}
    }
    set length 0
    regexp -nocase {Content-Length: ([0-9]+)} $headers -> length
    set body [string range $::input($channel) [expr {$end + 4}] end]
    if {[string length $body] < $length} {return}
    set body [string range $body 0 [expr {$length - 1}]]
    puts -nonewline $channel "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: $length\r\nConnection: close\r\n\r\n"
    puts -nonewline $channel $body
    close $channel
    unset ::input($channel)
}
set gates {}
set server [socket -server accept -myaddr 127.0.0.1 0]
puts [lindex [chan configure $server -sockname] 2]
flush stdout
fileevent stdin readable {if {[gets stdin] < 0 && [eof stdin]} {exit}}
vwait forever
