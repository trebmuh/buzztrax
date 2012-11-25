#!/bin/sh
# run buzztard-cmd --command=info on all example and test for crashes

cd tests
. ./bt-cfg.sh

E_SONGS="$TESTSONGDIR/buzz*.xml \
    $TESTSONGDIR/combi*.xml \
    $TESTSONGDIR/melo*.xml \
    $TESTSONGDIR/samples*.bzt \
    $TESTSONGDIR/simple*.xml \
    $TESTSONGDIR/simple*.bzt \
    $TESTSONGDIR/broken2.xml \
    $TESTSONGDIR/test*.xml"

T_SONGS="$TESTSONGDIR/broken1.xml \
    $TESTSONGDIR/broken1.bzt \
    $TESTSONGDIR/broken3.xml"

rm -f /tmp/bt_cmd_info.log
mkdir -p $TESTRESULTDIR
res=0

trap crashed SIGTERM SIGSEGV
crashed()
{
    echo "!!! crashed"
    res=1
}

# test working examples
for song in $E_SONGS; do
  if [ "$BT_CHECKS" != "" ]; then
    ls -1 $BT_CHECKS | grep >/dev/null $song
    if [ $? -eq 1 ]; then
      continue
    fi
  fi
  
  echo "testing $song"
  info=`basename $song .xml`
  info="$TESTRESULTDIR/$info.txt"
  echo >>/tmp/bt_cmd_info.log "== $song =="
  GST_DEBUG_NO_COLOR=1 GST_DEBUG="*:2,bt-*:4" libtool --mode=execute $BUZZTARD_CMD >$info 2>>/tmp/bt_cmd_info.log --command=info --input-file=$song
  if [ $? -ne 0 ]; then
    echo "!!! failed"
    res=1
  fi
done

# test failure cases
for song in $T_SONGS; do
  if [ "$BT_CHECKS" != "" ]; then
    ls -1 $BT_CHECKS | grep >/dev/null $song
    if [ $? -eq 1 ]; then
      continue
    fi
  fi

  echo "testing $song"
  info=`basename $song .xml`
  info="$TESTRESULTDIR/$info.txt"
  echo >>/tmp/bt_cmd_info.log "== $song =="
  GST_DEBUG_NO_COLOR=1 GST_DEBUG="*:2,bt-*:4" libtool --mode=execute $BUZZTARD_CMD >$info 2>>/tmp/bt_cmd_info.log --command=info --input-file=$song
  if [ $? -eq 0 ]; then
    echo "!!! failed"
    res=1
  fi
done

exit $res;

