#!/bin/sh

OUTPUT_FILE_NAME=results.txt

echo "aaa"
echo "bbb"
cd sample_dir
#create empty file

addRemoveSingleFile () {
	local FILE_NAME=bar
	touch $FILE_NAME
	echo -n "touch_SINGLE_FILE[">>$OUTPUT_FILE_NAME
	if [ -f $FILE_NAME ]; then echo "pass]">>$OUTPUT_FILE_NAME; else echo "fail]">>$OUTPUT_FILE_NAME; fi
	rm $FILE_NAME
	echo -n "rm_SINGLE_FILE[">>$OUTPUT_FILE_NAME
	if [ ! -f $FILE_NAME ]; then echo "pass]">>$OUTPUT_FILE_NAME; else echo "fail]">>$OUTPUT_FILE_NAME; fi
}

appendToFile () {
	local FILE_NAME=bar
	local i=0
	local F1=$FILE_NAME$i
	i=$((i+1))
	local F2=$FILE_NAME$i
	local STR="aaa"
	local LEN_STR=`echo $STR | wc -c`

	#touch $F1
	echo $STR>$F1;
	echo -n "FILE_APPEND_LEN[">>$OUTPUT_FILE_NAME
	local LEN_F1=`stat --printf="%s" $F1`
	if [ $LEN_STR -eq $LEN_F1 ]; then echo "pass]">>$OUTPUT_FILE_NAME; else echo "fail]$LEN_STR!=$LEN_F1">>$OUTPUT_FILE_NAME; fi
	
	echo $STR>>$F2;
	echo -n "DIFF_SAME_FILE[">>$OUTPUT_FILE_NAME
	local DIFF_COUNT=`diff $F1 $F2| wc -c`
	if [ 0 -eq $DIFF_COUNT ]; then echo "pass]">>$OUTPUT_FILE_NAME; else echo "fail]$DIFF_COUNT">>$OUTPUT_FILE_NAME; fi
	
	rm $F2
	rm $F1
}

addRemoveSingleFile
appendToFile

#make clean
#make

