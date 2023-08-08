################################################
#### Step 1: Produce our little console app ####
################################################
APP_NAME="disassembler"
COMPILER_OPTIONS="-fsanitize=address -g -o0 -Wall -Wfatal-errors -x c -std=c99"
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
if nasm -O0 src/samplemovs.asm -o build/machinecode; then
echo "nasm success"
else
exit 0
fi


###################################################
#### Step 3: Run
###################################################
if build/$APP_NAME > build/output.txt; then
    echo "program exited succesfully"
    if nasm build/output.txt -o build/machinecode2; then
    echo "nasm recompile success"
    else
    exit 0
    fi
    diff -i build/machinecode build/machinecode2
else
    echo "program exit code was 1 (failure)"
    cat build/output.txt
fi

