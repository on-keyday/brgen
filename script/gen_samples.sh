for file in example/*.bgn; do 
name=`basename $file`
tool/src2json $file > example/s2j_test/${name}.json 
done