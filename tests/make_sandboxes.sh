# ----------------------------------------------------------------------------
#!/bin/bash
# installs MySQL Tests instances
MYSQL_VERSION="$1"
[ -z "$MYSQL_VERSION" ] && MYSQL_VERSION=5.7.18
[ -z "$SANDBOX_HOME" ] && SANDBOX_HOME=$HOME/sandboxes
[ -z "$SANDBOX_BINARY" ] && SANDBOX_BINARY=$HOME/opt/mysql

if [ ! -f "$SANDBOX_BINARY/$MYSQL_VERSION/share/innodb_memcached_config.sql" ]; then 
	echo "no $SANDBOX_BINARY/$MYSQL_VERSION/share/innodb_memcached_config.sql found"
	exit 1
fi

make_multiple_sandbox --gtid --how_many_nodes=4 --group_directory=MYSQLND_MS "$MYSQL_VERSION"

if [ "$?" != "0" ] ; then exit 1 ; fi

multi_sb=$SANDBOX_HOME/MYSQLND_MS

baseport=$($multi_sb/n1 -BN -e 'select @@port')
memcport=$(($baseport+7905))
myport=$memcport
for N in 1 2 3 4
do
	$multi_sb/n$N <$SANDBOX_BINARY/$MYSQL_VERSION/share/innodb_memcached_config.sql
    options=(
        plugin-load=libmemcached.so
        daemon_memcached_option="-p$myport"
        session_track_gtids=1
    )
    $multi_sb/node$N/add_option ${options[*]}
    myport=$(($myport+1))
done

$multi_sb/use_all 'reset master'

user_cmd="CHANGE MASTER TO MASTER_USER='rsandbox', "
user_cmd="$user_cmd MASTER_PASSWORD='rsandbox', master_host='127.0.0.1', "
user_cmd="$user_cmd master_port=$(($baseport+1));"
user_cmd="$user_cmd START SLAVE;"    
$multi_sb/n1 -v -u root -e "$user_cmd"

user_cmd="CHANGE MASTER TO MASTER_USER='rsandbox', "
user_cmd="$user_cmd MASTER_PASSWORD='rsandbox', master_host='127.0.0.1', "
user_cmd="$user_cmd master_port=$baseport;"
user_cmd="$user_cmd START SLAVE;"    
$multi_sb/n2 -v -u root -e "$user_cmd"
SCRIPTPATH=$( cd "$(dirname "$0")" ; pwd -P )
sed -i -e "s/\(\s*\)\$baseport\s*=.*/\1\$baseport=$baseport;/" $SCRIPTPATH/config.inc
