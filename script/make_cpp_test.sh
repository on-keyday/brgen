#!/bin/bash

TARGET_DIR=`realpath $1`

if [ `uname -o` == 'Msys' ]; then
    # replace /c/ to C:/
    TARGET_DIR=`echo $TARGET_DIR | sed -e "s@^/c/@C:/@"`
fi

TARGET_PREFIX=$2

TARGET_HPP=$TARGET_DIR/$TARGET_PREFIX.hpp
TARGET_MAP=$TARGET_DIR/$TARGET_PREFIX.json

TEST_TEMPLATE=`pwd`/testkit/test_template.cpp
TEST_CMAKE_LISTS=`pwd`/testkit/CMakeLists.txt

TEMPLATE=`cat $TEST_TEMPLATE | sed -e "s@stub.hpp@$TARGET_HPP@"`
CMAKE_FILE=`cat $TEST_CMAKE_LISTS`

STRUCT_LIST=`cat $TARGET_MAP | jq -r '.structs[]'`

if [ `uname -o` == 'Msys' ]; then
    # replace \r\n to \n
    STRUCT_LIST=`echo -n "$STRUCT_LIST" | sed -e "s@\r\n@\n@"` 
fi

if [ -z "$STRUCT_LIST" ]; then
    echo "No struct found in $TARGET_MAP"
    exit 1
fi

TEST_DIR=$TARGET_DIR/${TARGET_PREFIX}_test
mkdir -p $TEST_DIR/src
CMAKE_FILE+='

'
for STRUCT in $STRUCT_LIST; do
    echo "$STRUCT"
    TEST_CODE=`echo "$TEMPLATE" | sed -e "s@TEST_CLASS@$STRUCT@"`
    #echo "$TEST_CODE"
    TO=${TEST_DIR}/src/${STRUCT}_test.cpp
    echo "$TEST_CODE" > $TO
    echo "create $TO"

    CMAKE_FILE+=`cat << EOF
add_executable(${STRUCT}_test "src/${STRUCT}_test.cpp")
target_link_libraries(${STRUCT}_test futils)
add_test(NAME ${STRUCT}_test COMMAND ${STRUCT}_test "$TEST_DIR/testData/$STRUCT.bin")
EOF`
    CMAKE_FILE+='
'
done

CMAKE_FILE+=`cat << EOF

# copy libfutils.dll or libfutils.dylib or libfutils.so to the same directory as the executable
if(WIN32)
    add_custom_command(TARGET ${STRUCT}_test POST_BUILD
        COMMAND \\\${CMAKE_COMMAND} -E copy_if_different
        "\\\$ENV{FUTILS_DIR}/lib/libfutils.dll"
        "\\\${CMAKE_BINARY_DIR}")
elseif(APPLE)
    add_custom_command(TARGET ${STRUCT}_test POST_BUILD
        COMMAND \\\${CMAKE_COMMAND} -E copy_if_different
        "\\\$ENV{FUTILS_DIR}/lib/libfutils.dylib"
        "\\\${CMAKE_BINARY_DIR}")
else()
    add_custom_command(TARGET ${STRUCT}_test POST_BUILD
        COMMAND \\\${CMAKE_COMMAND} -E copy_if_different
        "\\\$ENV{FUTILS_DIR}/lib/libfutils.so"
        "\\\${CMAKE_BINARY_DIR}")
endif()
EOF
`

echo "$CMAKE_FILE" > $TEST_DIR/CMakeLists.txt
echo "create $TEST_DIR/CMakeLists.txt"

BUILD_SH=`cat << EOF
#!/bin/bash
if [ -z "\$FUTILS_DIR" ]; then
    echo "FUTILS_DIR is not set"
    return 1
fi
cmake -S ${TEST_DIR} -B ${TEST_DIR}/built -G Ninja -DCMAKE_BUILD_TYPE=Debug 
cmake --build ${TEST_DIR}/built
if [ \\$? -ne 0 ]; then
    echo "build failed"
    return 1
fi
ctest --test-dir ${TEST_DIR}/built --output-on-failure
if [ \\$? -ne 0 ]; then
    echo "test failed"
    return 1
fi
echo "test success"
EOF`

echo "$BUILD_SH" > $TEST_DIR/build.sh
echo "create $TEST_DIR/build.sh"
chmod +x $TEST_DIR/build.sh
