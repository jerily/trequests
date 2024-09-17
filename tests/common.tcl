
tcltest::testConstraint testingModeEnabled [trequests::pkgconfig get testing-mode]
tcltest::testConstraint testingModeDisabled [expr { ![trequests::pkgconfig get testing-mode] }]

# Pre-create a default pool to avoid memory leaks, report memory regions
# associated with the async pool that was created during test execution
# and will be freed on exit.

catch {
    set r [::trequests::get http://localhost -async]
    $r destroy
    unset r
}
