#!/bin/bash
licensed cache
licensed status
if [ $? -eq 0 ]; then
  echo "licensed status: OK"
else
  echo "licensed status: FAIL"
  return 1
fi
go-licenses save ./... --save_path=license_cache/go --force --alsologtostderr --ignore=./ignore
if [ $? -eq 0 ]; then
  echo "go-licenses save: OK"
else
  echo "go-licenses save: FAIL"
  return 1
fi
