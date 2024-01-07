#!/bin/bash
licensed cache
licensed status
if [ $? -eq 0 ]; then
  echo "licensed status: OK"
else
  echo "licensed status: FAIL"
  return 1
fi
if [ ! -d ./license_cache ]; then
  mkdir ./license_cache
fi
if [ ! -d ./license_cache/go ]; then
  mkdir ./license_cache/go
fi
if [ ! -d ./license_cache/web ]; then
  mkdir ./license_cache/web
fi
GOCREDITS=$GOPATH/bin/gocredits
$GOCREDITS > ./license_cache/go/credits.txt
if [ $? -eq 0 ]; then
  echo "gocredits save: OK"
else
  echo "gocredits save: FAIL"
  return 1
fi
cd web/doc
$GOCREDITS > ../../license_cache/web/credits.txt
RESULT=$?
cd ../..
if [ $RESULT -eq 0 ]; then
  echo "gocredits save: OK"
else
  echo "gocredits save: FAIL"
  return 1
fi
cp ./script/license_note.txt ./license_cache/
if [ $? -eq 0 ]; then
  echo "license_note.txt copy: OK"
else
  echo "license_note.txt copy: FAIL"
  return 1
fi
