################################################
#### Step 1: Produce our little console app ####
################################################
APP_NAME="disassembler"
COMPILER_OPTIONS="-Wall -x c -std=c99"
SOURCE="src/main.c"

rm -r build
mkdir -p build

if gcc $COMPILER_OPTIONS $SOURCE -o build/$APP_NAME; then
echo "gcc success"
else
exit 0
fi


###################################################
#### Step 2: Produce sample 8086 machine code  ####
###################################################
if nasm src/samplemovs.asm -o build/machinecode; then
echo "nasm success"
else
exit 0
fi


###################################################
#### Step 3: Run
###################################################
build/$APP_NAME

