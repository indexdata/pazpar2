#!/bin/sh
#
# pazpar2.sh - pazpar2 start/stop script

: ${PAZPAR2_HOME=$HOME/pazpar2}
: ${pazpar2_program=$PAZPAR2_HOME/src/pazpar2}
: ${pazpar2_config=$PAZPAR2_HOME/etc/pazpar2.cfg}
: ${pazpar2_pid=$PAZPAR2_HOME/pazpar2.pid}
: ${pazpar2_log=$PAZPAR2_HOME/pazpar2.log}

command=$1; shift

case "$command" in
	start)
		$pazpar2_program -D -l $pazpar2_log -p $pazpar2_pid -f $pazpar2_config "$@"
        	;;

	stop) 
		test -f $pazpar2_pid && kill `cat $pazpar2_pid`
        	;;

	# graceful restart - not yet implemented by pazpar2
	graceful)
		test -f $pazpar2_pid && kill -HUP `cat $pazpar2_pid`
        	;;

	restart)
		$0 stop
		$0 start "$@"
		;;

	*)
        echo "Usage: `basename $0` [ start [pazpar2 options]] [ stop | restart ]" >&2
        ;;
esac

exit 0

