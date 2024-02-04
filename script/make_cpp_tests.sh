
if [ $# -ne 1 ]; then
    echo "Usage: $0 <test_info_file>"
    exit 1
fi

TEST_TARGETS=`cat $1 | jq -r  '.generated_files[] | select(.suffix==".hpp") | "\(.dir) \(.base)"'`

if [ `uname -o` == 'Msys' ]; then
    # replace \r\n to \n
    TEST_TARGETS=`echo -n "$TEST_TARGETS" | sed -e "s@\r\n@\n@"` 
fi

MAKE_CPP_TEST_PATH=`cd $(dirname ${0}) && pwd`/make_cpp_test.sh

echo "$MAKE_CPP_TEST_PATH"
RUN_BUILD=$2
IFS=$'\n'
for TEST_TARGET in $TEST_TARGETS; do
    DIR=`echo $TEST_TARGET | cut -d ' ' -f 1`
    BASE=`echo $TEST_TARGET | cut -d ' ' -f 2`
    echo "make test for $DIR/$BASE.hpp" 
    bash -C $MAKE_CPP_TEST_PATH $DIR $BASE $RUN_BUILD &
done
wait
echo "all tests are done"
