# connect
proc cn { } {
    connect -host localhost -port 3121
    targets -set -filter {name =~ "ARM* #0"}
}

# reload
proc rl { } {
    rst
    dow bin/fsbl.elf
    con 
}

# restart
proc rs { } {
    rl
    con 
}
