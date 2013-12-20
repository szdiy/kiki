#
#!/bin/bash
#

# Anders 2011_0921

if [ $# != 3 ]; then
	echo "Usage: log.sh [rfid] [name] [time]"
	exit 1
fi

DATE=`date +%Y-%m-%d`
URL="http://www.szdiy.org/party.php"
NAME=$2
PASSWORD="11805812962901298014026"
URL=$URL"?hacker="$NAME"&key="$PASSWORD 
MAXPOST=5

#post data to website
retry=0
while [ $retry -lt $MAXPOST ]
do
	echo "posting $2 ... "$retry
	#echo $URL
	curl -s $URL | grep -F '<!--SZDIY.ORG -->'
	if [ $? -ne 0 ]; then
		let retry=$retry+1
		sleep 1s;
		curl $URL | grep -F '<!--SZDIY.ORG -->'
	else
		break
	fi
done 

if [ $retry -eq $MAXPOST ]; then
	DATE=$DATE".error"
fi

#save log
if [ ! -e log ];then
	mkdir log
fi
if [ ! -e log/$DATE ];then
	touch log/$DATE
fi
echo $1"%"$2"%"$3 >> log/$DATE
