#
# Network CTables
#
# Client-Side
#
#
# $Id$
#

package require ctable_net
package require Tclx

#
# remote_ctable - declare a ctable as going to a remote host
#
# remote_ctable $serverHost myTable
#
#  myTable is now a command that works like a ctable except it's all
#  client server behind your back.
#
proc remote_ctable {cttpUrl localTableName} {
    variable ctableUrls
    variable ctableLocalTableUrls

#puts stderr "define remote ctable: $localTableName -> $cttpUrl"

    set ctableUrls($localTableName) $cttpUrl
    set ctableLocalTableUrls($cttpUrl) $localTableName

    proc $localTableName {args} "
	set level \[info level]; incr level -1
	remote_ctable_invoke $localTableName \$level \$args
    "
}

#
# remote_ctable_destroy - destroy a remote ctable connection
#
proc remote_ctable_destroy {cttpUrl} {
    variable ctableUrls
    variable ctableLocalTableUrls

    if [info exists ctableLocalTableUrls($cttpUrl)] {
	# puts [list rename $ctableLocalTableUrls($cttpUrl) ""]
	rename $ctableLocalTableUrls($cttpUrl) ""
	if [info exists ctableUrls($ctableLocalTableUrls($cttpUrl))] {
	    unset ctableUrls($ctableLocalTableUrls($cttpUrl))
	}
	unset ctableLocalTableUrls($cttpUrl)
    }
    # puts [list remote_ctable_cache_disconnect $cttpUrl]
    remote_ctable_cache_disconnect $cttpUrl
}

#
# remote_ctable_cache_disconnect - disconnect from a remote ctable server
#
proc remote_ctable_cache_disconnect {cttpUrl} {
    variable ctableSockets

    if {[info exists ctableSockets($cttpUrl)]} {
	set oldsock $ctableSockets($cttpUrl)
	unset ctableSockets($cttpUrl)
	catch {close $oldsock}
    }
}

#
# remote_ctable_connect - connect to a remote ctable server
#
proc remote_ctable_cache_connect {cttpUrl} {
    variable ctableSockets

    # If there's a valid open socket
    if {[info exists ctableSockets($cttpUrl)]} {
	set oldsock $ctableSockets($cttpUrl)
	if {![eof $oldsock]} {
            return $oldsock
	}
	unset ctableSockets($cttpUrl)
	catch {close $oldsock}
    }

    lassign [::ctable_net::split_ctable_url $cttpUrl] host port dir remoteTable stuff

    # Don't error out immediately if we can't open the socket
    if [catch {socket $host $port} sock] {
	after 500
	set sock [socket $host $port]
    }

    # Previous code retried this. I don't see any reason to, I think it was
    # trying to handle the "can't open socket" case.
    if {[gets $sock line] < 0} {
	close $sock
	error "unexpected EOF from server on connect"
    }

    if {[lindex $line 0] != "ctable_server" && [lindex $line 0] != "sttp_server"} {
	close $sock
	error "server hello line format error"
    }

    if {[lindex $line 1] != "1.0"} {
	close $sock
	error "server version [lindex $line 1] mismatch"
    }

    if {[lindex $line 2] != "ready"} {
	close $sock
	error "unable to handle server state of unreadiness"
    }

    set ctableSockets($cttpUrl) $sock
    return $sock
}

#
# remote_sock_send - send a command over a socket
#
# Multi-line commands are sent as a line containing "# NNNN" followed by
# NNNN bytes
#
proc remote_sock_send {sock cttpUrl command} {
    set line [list $cttpUrl $command]
    if [string match "*\n*" $line] {
	puts $sock [list # [expr [string length $line] + 1]]
    }
    puts $sock [list $cttpUrl $command]
    flush $sock
}

#
# read a possibly multi-line response from the server
#
proc get_response {sock lineVar} {
    upvar 1 $lineVar line
    while {[set length [gets $sock line]] == 0} { continue }
    if {$length > 0} {
        # Handle "# NNNN" - multi-line request NNNN bytes long
        if {"[string index $line 0]" == "#"} {
	    set length [string trim [string range $line 1 end]]
	    set line [read $sock $length]
	    if {"$line" == ""} {
		set length 0
	    }
        }
    }
    return $length
}

#
# remote_ctable_send - send a command to a remote ctable server
#
proc remote_ctable_send {cttpUrl command {actionData ""} {callerLevel ""} {no_redirect 0}} {
    variable ctableSockets
    variable ctableLocalTableUrls

#puts "actionData '$actionData'"

    set sock [remote_ctable_cache_connect $cttpUrl]

    # Try 5 times to send the data and get a response
    set i 0
    while 1 {
        if {[catch {remote_sock_send $sock $cttpUrl $command} err] != 1} {
	    if {[get_response $sock line] > 0} { break }
	    set err "Unexpected EOF from server on response"
	}
	incr i
	if {$i > 5} {
	    error "$cttpUrl: $err"
	}
        remote_ctable_cache_disconnect $cttpUrl
    
	set sock [remote_ctable_cache_connect $cttpUrl]
    }

    while 1 {
	switch [lindex $line 0] {
	    "e" {
		error [lindex $line 1] [lindex $line 2] [lindex $line 3]
	    }

	    "k" {
		return [lindex $line 1]
	    }

	    "r" {
		if $no_redirect {
		    if {"$command" == "redirect"} {
		        error "Redirected for redirect to [lindex $line 1]"
		    }
		    return ""
		}
# puts "[clock format [clock seconds]] redirect '$line'"
# parray ctableLocalTableUrls
		remote_ctable_cache_disconnect $cttpUrl
		set newCttpUrl [lindex $line 1]
		if {[info exists ctableLocalTableUrls($cttpUrl)]} {
		    set localTable $ctableLocalTableUrls($cttpUrl)
		    unset ctableLocalTableUrls($cttpUrl)
		    remote_ctable $newCttpUrl $localTable
		}
# puts "[clock format [clock seconds]] retry $newCttpUrl '$command'"
		return [remote_ctable_send $newCttpUrl $command $actionData $callerLevel]
	    }

	    "m" {
		array set actions $actionData
		set firstLine 1

		set result ""
		while {[gets $sock line] >= 0} {
		    if {$line == "\\."} {
			break
		    }
#puts "processing line '$line'"

		    if {[info exists actions(-write_tabsep)]} {
			set status [catch {
			    puts $actions(-write_tabsep) $line
			} error]
			if {$status == 1} {
			    set savedInfo $::errorInfo
			    set savedCode $::errorCode
			    remote_ctable_cache_disconnect $cttpUrl
			    error $result $savedInfo $savedCode
			}
		    } elseif {[info exists actions(-code)]} {
			if {$firstLine} {
			    set firstLine 0
			    set fields $line
			    continue
			}

#puts "fields '$fields' value '$line'"
			set dataList ""
			foreach var $fields value [split $line "\t"] {
#puts "var '$var' value '$value"
			    if {$var == "_key"} {
				if {[info exists actions(keyVar)]} {
#puts "set $actions(keyVar) $value"
				    uplevel #$callerLevel set $actions(keyVar) $value
				}
#
# TODO - make sure the behavior with -get is consistent
#
				if {$actions(action) != "-get" && [info exists actions(-noKeys)] && $actions(-noKeys) == 1} {
				    continue
				}
			    }

			    if {$actions(action) == "-get"} {
				lappend dataList $value
			    } else {
			        # it's -array_get, -array_get_with_nulls,
				# -array or -array_with_nulls
				lappend dataList $var $value
			    }
			}

			if {$actions(action) == "-array" || $actions(action) == "-array_with_nulls"} {
			    set dataCmd [list array set $actions(bodyVar) $dataList]
			} else {
			    set dataCmd [list set $actions(bodyVar) $dataList]
			}

#puts "executing '$dataCmd'"
#puts "executing '$actions(-code)'"

			set status [catch {
			    uplevel #$callerLevel "
			        $dataCmd
			        $actions(-code)
			    "
			} result]

			# TCL_ERROR
			if {$status == 1} {
			    set savedInfo $::errorInfo
			    set savedCode $::errorCode
			    remote_ctable_cache_disconnect $cttpUrl
			    error $result $savedInfo $savedCode
			}

 			# TCL_RETURN/TCL_BREAK
			if {$status == 2 || $status == 3} {
			    remote_ctable_cache_disconnect $cttpUrl
			    return $result
			}

			# TCL_OK or TCL_CONTINUE just keep going
		    } else {
			error "no action, need -write_tabsep or -code: $actionData"
		    }
		}
		if {[get_response $sock line] <= 0} {
		    error "$cttpUrl: unexpected EOF from server after multiline response"
		}
		return $result
	    }

	    default {
		error "unknown command response '$line'"
	    }
	}
    }
}

#
# remote_ctable_create - create on the specified host an instance of the ctable creator creatorName named tableName
#
proc remote_ctable_create {cttpUrl creatorName remoteTableName} {
    return [remote_ctable_send $cttpUrl [list create $creatorName $remoteTableName]]
}

#
# remote_ctable_invoke - object handler for procs generated by remote_ctable
#
proc remote_ctable_invoke {localTableName level command} {
    variable ctableSockets
    variable ctableUrls
    variable ctableLocalTableUrls

    set cttpUrl $ctableUrls($localTableName)

    set cmd [lindex $command 0]
    set body [lrange $command 1 end]

#puts "cmd '$command', pairs '$body'"

    # Have to handle "destroy" specially - don't pass to far end, just
    # close the socket and destroy myself
    if {"$cmd" == "destroy"} {
#puts "command is destroy"
	return [remote_ctable_destroy $cttpUrl]
    }

    # If the comand is "redirect" or "shutdown", don't follow redirects
    set no_redirect [expr {"$cmd" == "redirect" || "$cmd" == "shutdown"}]

    # if it's search, take out args that will freak out the remote side
    if {$cmd == "search" || $cmd == "search+"} {
	array set pairs $body
	if {[info exists pairs(-write_tabsep)]} {
	    set actions(-write_tabsep) $pairs(-write_tabsep)
	    unset pairs(-write_tabsep)
	}

	if {[info exists pairs(-noKeys)]} {
	    set actions(-noKeys) $pairs(-noKeys)
	}

	if {[info exists pairs(-code)]} {
	    set actions(-code) $pairs(-code)
	    unset pairs(-code)

	    foreach var {-key -array_get -array_get_with_nulls -array -array_with_nulls -get} {
		if {[info exists pairs($var)]} {
		    set actions(action) $var
		    set actions($var) $pairs($var)
		    unset pairs($var)

		    if {$var == "-key"} {
			set actions(keyVar) $actions($var)
		    } else {
			set actions(bodyVar) $actions($var)
		    }
		}
	    }

	    set pairs(-with_field_names) 1
	}
	set body [array get pairs]
#puts "new body is '$body'"
#puts "new actions is [array get actions]"
    }

    return [remote_ctable_send $cttpUrl [linsert $body 0 $cmd] [array get actions] $level $no_redirect]
}

package provide ctable_client 1.0

