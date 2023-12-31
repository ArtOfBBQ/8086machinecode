#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

typedef struct ParsedLines {
    uint32_t machine_bytes;
    char parsed_text[800];
    int32_t label_id;
    int32_t append_jump_bytes;
    int32_t jump_targets_label_id;
} ParsedLines;

#define PARSED_LINES_MAX 10000
static ParsedLines * parsed_lines = NULL;
static uint32_t parsed_lines_size = 0;
static uint32_t latest_label_id = 0;

static void print_binary(
    const uint8_t input)
{
    for (int32_t i = 7; i >= 0; i--) {
        printf("%u", (input >> i) & 1);
    }
}

static void read_file(
    char * filename,
    uint8_t * recipient,
    uint32_t * recipient_size,
    uint32_t recipient_cap)
{
    FILE * fileptr = fopen(filename, "rb"); 
    if (fileptr != NULL) {
        size_t elements_read = fread(
            recipient,
            1,
            recipient_cap,
            fileptr);
        
        *recipient_size = (uint32_t)elements_read;
        
        fclose(fileptr);
    }
}

static void strcat(
    char * recipient,
    char * to_cat)
{
    uint32_t i = 0;
    while (recipient[i] != '\0') {
        i++;
    }
    
    uint32_t j = 0;
    while (to_cat[j] != '\0') {
        recipient[i++] = to_cat[j++];
    }
    
    recipient[i] = '\0';
}

static void strcpy(
    char * recipient,
    char * to_copy)
{
    recipient[0] = '\0';
    strcat(recipient, to_copy);
}

static void strcat_uint(
    char * recipient,
    uint16_t to_cat)
{
    uint32_t i = 0;
    while (recipient[i] != '\0') {
        i++;
    }
    
    uint32_t found_leader = 0;
    
    uint32_t mod = 100000000;
    while (mod > 0) {
        uint32_t new_digit = to_cat / mod;
        assert(new_digit < 10);
        to_cat -= (new_digit * mod);
        
        if (new_digit > 0 || found_leader) {
            found_leader = 1;
            recipient[i++] = '0' + new_digit;
        }
        mod /= 10;
    }
    
    if (!found_leader) {
        recipient[i++] = '0';
    }
    
    recipient[i] = '\0';
}

static void strcat_binary_uint(
    char * recipient,
    uint8_t to_cat,
    uint8_t digits)
{
    for (int32_t i = (digits - 1); i >= 0; i--) {
        strcat_uint(recipient, (to_cat >> i) & 1);
    }
}

static void strcat_int(
    char * recipient,
    int16_t to_cat)
{
    if (to_cat < 0) {
        strcat(recipient, "-");
        to_cat *= -1;
    }
    strcat_uint(recipient, (uint16_t)to_cat);
}

/*
From 1+ bytes of machine code, we will get an opcode, a 'W' flag, a 'D' flag,
a 'mod' field, a 'reg' field and an 'r_m' field.

Which fields you expect to see are always different depending on the opcode,
but the order of fields always seems to be the same, and the fields are always
the same number of bits

Here is an example of 1 of the MOV opcodes' signatures:

####################################################
# Byte 1                 ## Byte 2                 #
#//////////////////      ##////////////////////////#
#/0  1  2  3  4  5/ 6  7 ##/8  7/ 6  5  4/ 3  2  1/#
#/#######/########/########/####/########/########/#
 / O P C O D E    / W  D   / MOD/ R E G  /  R_M   / 
 //////////////////        //////////////////////// 
*/

/*
The OPCODE is a 3 to 8 bits and represents the 1st part of the
assembly instruction
*/
#define MOV_REGMEMTOREG        34 // binary: 100010
#define MOV_IMMTOREG           11 // binary: 1011
#define MOV_IMMTOREGMEM        99 // binary: 1100011

/*
note: these names are from the intel manual, actually I would reverse them
('memory to accumulator' actually moves what's in the accumulator to memory)
*/
#define MOV_MEMTOACC           80 // binary: 1010000
#define MOV_ACCTOMEM           81 // binary: 1010001


#define ADD_REGMEMTOREG         0 // binary: 000000
#define ADD_IMMTOREGMEM        32 // binary: 100000
#define ADD_IMMTOACC            2 // binary: 0000010

#define SUB_REGMEMTOREG        10 // binary: 001010
#define SUB_IMMTOREGMEM        32 // binary: 100000
#define SUB_IMMTOACC           22 // binary: 0010110

#define CMP_REGMEMTOREG        14 // binary: 001110
#define CMP_IMMTOREGMEM        32 // binary: 100000
#define CMP_IMMTOACC           30 // binary: 0011110

// #define RET_WITHINSEGMENT  195 // 11000011

#define JO                    112 // 01110000 (jump on overflow)
#define JNO                   113 // 01110001 (jump on not overflow)
#define JB                    114 // 01110010 (jump below)
#define JNB                   115 // 01110011 (jump not below)
#define JE                    116 // 01110100 (jump equal)
#define JNE_JNZ               117 // 01110101 (jump not equal, jump not zero)
#define JBE_JNA               118 // 01110110 (below equal, not above)
#define JA                    119 // 01110111 (jump above)
#define JS                    120 // 01111000 (jump on sign)
#define JNS                   121 // 01111001 (?)
#define JP                    122 // 01111010 (jump on parity)
#define JNP                   123 // 01111011 (jump on not parity)
#define JL                    124 // 01111100 (jump less)
#define JNL                   125 // 01111101 (jump not less)
#define JLE                   126 // 01111110 (jump less or equal)
#define JG                    127 // 01111111 (jump greater)
#define LOOPNZ_LOOPNE         224 // 11100000 (loop while not 0 / not equal)
#define LOOPZ_LOOPE           225 // 11100001 (loop while 0 / while equal)
#define LOOP                  226 // 11100010 (loop cx times)
#define JCXZ                  227 // 11100011 (jump when cx is 0)

typedef struct OpCode {
    char text[10];
    uint8_t number;
    uint8_t size_in_bits;
    uint8_t has_secondary_3bit_opcode;
    uint8_t secondary_3bit_opcode;
    uint32_t secondary_3bit_offset;
    uint8_t has_d_field;
    uint8_t hardcoded_d_field; // this opcode always behaves as if d = x
    uint8_t has_s_field;
    uint8_t has_w_field;
    uint8_t has_mod;
    uint8_t has_reg;
    char hardcoded_reg_w[3]; // always use this register if w = 1
    char hardcoded_reg_b[3]; // always use this register if w = 0
    uint8_t has_rm;
    uint8_t data_bytes_are_addresses;
    uint8_t data_bytes_are_immediates;
    uint8_t data_bytes_are_jump_offsets;
    uint8_t has_data_byte_1;
    uint8_t has_data_byte_2_if_w;
    uint8_t has_data_byte_2_always;
} OpCode;

#define OPCODE_TABLE_SIZE 200
static OpCode * opcode_table = NULL;
static uint32_t opcode_table_size = 0;

/*
The reg field and r/m (register/memory) field refer to registers on the cpu

We will use 2 tables to transform the 3 bits in 'reg' and 'r_m' into
some characters of text (like "BP") that represent the register. The string
buffer only needs 3 characters

- This table is always used to decode the 'reg' field, regardless of the mod
- This table is also used to decode the 'r/m' if and only if mod = 3
is indexed by the value from the 'w' bit and then by 'r/m' to yield 3 chars
                      w  rm 3 chars
                      |  |  |
*/
static char reg_table[2][8][3];

/*
The modsub3 rm table shows the r/m for mods 'below 3' (so 00, 01, and 10)
If you are decoding the REG field, you DON'T use this table for any mod
it's indexed by the values in mod and r/m, and yields 15 chars
                            mod rm 15 chars
                             |  |  |
*/
static char modsub3_rm_table[3][8][15];

static void init_tables(void) {
    
    parsed_lines =
        (ParsedLines *)malloc(sizeof(ParsedLines) * PARSED_LINES_MAX);
    parsed_lines_size = 0;
    
    for (uint32_t i = 0; i < PARSED_LINES_MAX; i++) {
        parsed_lines[i].parsed_text[0] = '\0';
        parsed_lines[i].label_id = -1;
        parsed_lines[i].machine_bytes = 0;
        parsed_lines[i].append_jump_bytes = INT32_MIN;
        parsed_lines[i].jump_targets_label_id = -1;
    }
    
    opcode_table = (OpCode *)malloc(OPCODE_TABLE_SIZE * sizeof(OpCode));
    
    assert(OPCODE_TABLE_SIZE > 100);
    for (uint32_t i = 0; i < OPCODE_TABLE_SIZE; i++) {
        opcode_table[i].text[0] = '\0';
        opcode_table[i].size_in_bits = 0;
        opcode_table[i].has_secondary_3bit_opcode = false;
        opcode_table[i].secondary_3bit_opcode = 0;
        opcode_table[i].secondary_3bit_offset = 0;
        opcode_table[i].has_d_field = false;
        opcode_table[i].hardcoded_d_field = 0;
        opcode_table[i].has_s_field = false;
        opcode_table[i].has_w_field = false;
        opcode_table[i].has_mod = false;
        opcode_table[i].has_reg = false;
        opcode_table[i].hardcoded_reg_w[0] = '\0';
        opcode_table[i].hardcoded_reg_b[0] = '\0';
        opcode_table[i].has_rm = false;
        opcode_table[i].has_data_byte_1 = false;
        opcode_table[i].has_data_byte_2_if_w = false;
        opcode_table[i].has_data_byte_2_always = false;
        opcode_table[i].data_bytes_are_addresses = false;
        opcode_table[i].data_bytes_are_immediates = false;
        opcode_table[i].data_bytes_are_jump_offsets = false;
    }
    
    strcpy(opcode_table[opcode_table_size].text, "MOV");
    opcode_table[opcode_table_size].number = MOV_IMMTOREG; // 1011
    opcode_table[opcode_table_size].size_in_bits = 4;
    opcode_table[opcode_table_size].has_d_field = false;
    opcode_table[opcode_table_size].hardcoded_d_field = 1;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_mod = false;
    opcode_table[opcode_table_size].has_reg = true;
    opcode_table[opcode_table_size].has_rm = false;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].has_data_byte_2_if_w = true;
    opcode_table_size += 1;
    
    strcpy(opcode_table[opcode_table_size].text, "MOV");
    opcode_table[opcode_table_size].number = MOV_REGMEMTOREG; // 100010
    opcode_table[opcode_table_size].size_in_bits = 6;
    opcode_table[opcode_table_size].has_d_field = true;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_mod = true;
    opcode_table[opcode_table_size].has_reg = true;
    opcode_table[opcode_table_size].has_rm = true;
    opcode_table_size += 1;
    
    /* 
    note: these names are from the intel manual, actually I would reverse them
    ('memory to accumulator' moves what's in the accumulator to memory)
    
    mov [2555], ax
    */
    strcpy(opcode_table[opcode_table_size].text, "MOV");
    opcode_table[opcode_table_size].number = MOV_ACCTOMEM; // 1010001
    strcpy(opcode_table[opcode_table_size].hardcoded_reg_w, "AX");
    strcpy(opcode_table[opcode_table_size].hardcoded_reg_b, "AL");
    opcode_table[opcode_table_size].size_in_bits = 7;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].hardcoded_d_field = 0;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].has_data_byte_2_always = true;
    opcode_table[opcode_table_size].data_bytes_are_addresses = true;
    opcode_table_size += 1;
    
    /* 
    mov ax, [2555] (yes, intel's opcode name is reversed)
    */
    strcpy(opcode_table[opcode_table_size].text, "MOV");
    opcode_table[opcode_table_size].number = MOV_MEMTOACC; // 1010000
    strcpy(opcode_table[opcode_table_size].hardcoded_reg_w, "AX");
    strcpy(opcode_table[opcode_table_size].hardcoded_reg_b, "AL");
    opcode_table[opcode_table_size].size_in_bits = 7;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].hardcoded_d_field = 1;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].has_data_byte_2_always = true;
    opcode_table[opcode_table_size].data_bytes_are_addresses = true;
    opcode_table_size += 1;
    
    strcpy(opcode_table[opcode_table_size].text, "MOV");
    opcode_table[opcode_table_size].number = MOV_IMMTOREGMEM; // 1100011
    opcode_table[opcode_table_size].size_in_bits = 7;
    opcode_table[opcode_table_size].has_secondary_3bit_opcode = true;
    opcode_table[opcode_table_size].secondary_3bit_opcode = 0; // 000
    opcode_table[opcode_table_size].secondary_3bit_offset = 10;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_d_field = false;
    // opcode_table[opcode_table_size].hardcoded_d_field = 0;
    opcode_table[opcode_table_size].has_mod = true;
    opcode_table[opcode_table_size].has_reg = false;
    opcode_table[opcode_table_size].has_rm = true;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].has_data_byte_2_if_w = true;
    opcode_table[opcode_table_size].data_bytes_are_immediates = true;
    opcode_table_size += 1;
    
    strcpy(opcode_table[opcode_table_size].text, "ADD");
    opcode_table[opcode_table_size].number = ADD_REGMEMTOREG; // 000000
    opcode_table[opcode_table_size].size_in_bits = 6;
    opcode_table[opcode_table_size].has_d_field = true;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_mod = true;
    opcode_table[opcode_table_size].has_reg = true;
    opcode_table[opcode_table_size].has_rm = true;
    opcode_table_size += 1;
    
    strcpy(opcode_table[opcode_table_size].text, "ADD");
    opcode_table[opcode_table_size].number = ADD_IMMTOREGMEM; // 100000
    opcode_table[opcode_table_size].size_in_bits = 6;
    opcode_table[opcode_table_size].has_secondary_3bit_opcode = true;
    opcode_table[opcode_table_size].secondary_3bit_opcode = 0; // 100
    opcode_table[opcode_table_size].secondary_3bit_offset = 10;
    opcode_table[opcode_table_size].has_s_field = true;
    opcode_table[opcode_table_size].has_w_field = true;
    // opcode_table[opcode_table_size].hardcoded_d_field = 0;
    opcode_table[opcode_table_size].has_mod = true;
    opcode_table[opcode_table_size].has_reg = false;
    opcode_table[opcode_table_size].has_rm = true;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].has_data_byte_2_if_w = true;
    opcode_table[opcode_table_size].data_bytes_are_immediates = true;
    opcode_table_size += 1;
    
    strcpy(opcode_table[opcode_table_size].text, "SUB");
    opcode_table[opcode_table_size].number = SUB_REGMEMTOREG; // 001010
    opcode_table[opcode_table_size].size_in_bits = 6;
    opcode_table[opcode_table_size].has_d_field = true;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_mod = true;
    opcode_table[opcode_table_size].has_reg = true;
    opcode_table[opcode_table_size].has_rm = true;
    opcode_table_size += 1;
    
    strcpy(opcode_table[opcode_table_size].text, "SUB");
    opcode_table[opcode_table_size].number = SUB_IMMTOREGMEM; // 100000
    opcode_table[opcode_table_size].size_in_bits = 6;
    opcode_table[opcode_table_size].has_secondary_3bit_opcode = true;
    opcode_table[opcode_table_size].secondary_3bit_opcode = 5; // 101
    opcode_table[opcode_table_size].secondary_3bit_offset = 10;
    opcode_table[opcode_table_size].has_s_field = true;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].hardcoded_d_field = 0;
    opcode_table[opcode_table_size].has_mod = true;
    opcode_table[opcode_table_size].has_reg = false;
    opcode_table[opcode_table_size].has_rm = true;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].has_data_byte_2_if_w = true;
    opcode_table[opcode_table_size].data_bytes_are_immediates = true;
    opcode_table_size += 1;
    
    strcpy(opcode_table[opcode_table_size].text, "CMP");
    opcode_table[opcode_table_size].number = CMP_REGMEMTOREG; // 001110
    opcode_table[opcode_table_size].size_in_bits = 6;
    opcode_table[opcode_table_size].has_d_field = true;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_mod = true;
    opcode_table[opcode_table_size].has_reg = true;
    opcode_table[opcode_table_size].has_rm = true;
    opcode_table_size += 1;

    /*
    cmp si, 2
    */
    strcpy(opcode_table[opcode_table_size].text, "CMP");
    opcode_table[opcode_table_size].number = SUB_IMMTOREGMEM; // 100000
    opcode_table[opcode_table_size].size_in_bits = 6;
    opcode_table[opcode_table_size].has_secondary_3bit_opcode = true;
    opcode_table[opcode_table_size].secondary_3bit_opcode = 7; // 111
    opcode_table[opcode_table_size].secondary_3bit_offset = 10;
    opcode_table[opcode_table_size].has_s_field = true;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].hardcoded_d_field = 0;
    opcode_table[opcode_table_size].has_mod = true;
    opcode_table[opcode_table_size].has_reg = false;
    opcode_table[opcode_table_size].has_rm = true;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].has_data_byte_2_if_w = true;
    opcode_table[opcode_table_size].data_bytes_are_immediates = true;
    opcode_table_size += 1;

    /*
    cmp ax, 2
    */
    strcpy(opcode_table[opcode_table_size].text, "CMP");
    opcode_table[opcode_table_size].number = CMP_IMMTOACC; // binary: 0011110
    opcode_table[opcode_table_size].size_in_bits = 7;
    strcpy(opcode_table[opcode_table_size].hardcoded_reg_w, "AX");
    strcpy(opcode_table[opcode_table_size].hardcoded_reg_b, "AL");
    opcode_table[opcode_table_size].hardcoded_d_field = 1;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].has_data_byte_2_if_w = true;
    opcode_table[opcode_table_size].data_bytes_are_immediates = true;
    opcode_table_size += 1;
    
    /*
    add ax, 1000
    */
    strcpy(opcode_table[opcode_table_size].text, "ADD");
    opcode_table[opcode_table_size].number = ADD_IMMTOACC; // 0000010
    opcode_table[opcode_table_size].size_in_bits = 7;
    strcpy(opcode_table[opcode_table_size].hardcoded_reg_w, "AX");
    strcpy(opcode_table[opcode_table_size].hardcoded_reg_b, "AL");
    opcode_table[opcode_table_size].hardcoded_d_field = 1;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].has_data_byte_2_if_w = true;
    opcode_table[opcode_table_size].data_bytes_are_immediates = true;
    opcode_table_size += 1;

    /*
    sub ax, 1000
    */
    strcpy(opcode_table[opcode_table_size].text, "SUB");
    opcode_table[opcode_table_size].number = SUB_IMMTOACC; // binary: 0010110
    opcode_table[opcode_table_size].size_in_bits = 7;
    strcpy(opcode_table[opcode_table_size].hardcoded_reg_w, "AX");
    strcpy(opcode_table[opcode_table_size].hardcoded_reg_b, "AL");
    opcode_table[opcode_table_size].hardcoded_d_field = 1;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].has_data_byte_2_if_w = true;
    opcode_table[opcode_table_size].data_bytes_are_immediates = true;
    opcode_table_size += 1;
    
    /*
    jump not zero
    jnz test_label1
    */
    strcpy(opcode_table[opcode_table_size].text, "JNZ");
    opcode_table[opcode_table_size].number = JNE_JNZ; // binary: 01110101
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump equal
    je label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JE");
    opcode_table[opcode_table_size].number = JE; // binary: 01110100
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump on overflow
    jo label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JO");
    opcode_table[opcode_table_size].number = JO;
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump on overflow
    jno label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JNO");
    opcode_table[opcode_table_size].number = JNO;
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump below (TODO: understand how is this different from jump less)
    jb label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JB");
    opcode_table[opcode_table_size].number = JB; // binary: 01110010
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump not below
    jnb label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JNB");
    opcode_table[opcode_table_size].number = JNB;
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;

    /*
    jump if below or equal
    jbe label2
    */ 
    strcpy(opcode_table[opcode_table_size].text, "JBE");
    opcode_table[opcode_table_size].number = JBE_JNA; // binary: 01110110
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump abovej
    ja label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JA");
    opcode_table[opcode_table_size].number = JA;
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump on sign
    js label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JS");
    opcode_table[opcode_table_size].number = JS;
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;

    /*
    JNS (jump on not sign)
    */
    strcpy(opcode_table[opcode_table_size].text, "JNS");
    assert(opcode_table[opcode_table_size].text[0] != '\0');
    assert(!opcode_table[opcode_table_size].has_secondary_3bit_opcode);
    opcode_table[opcode_table_size].number = JNS;
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump parity
    jp label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JP");
    opcode_table[opcode_table_size].number = JP;
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump not parity
    jnp label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JNP");
    opcode_table[opcode_table_size].number = JNP;
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump less
    jl label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JL");
    opcode_table[opcode_table_size].number = JL; // binary: 01111100
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    jump not less
    jnl label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JNL");
    opcode_table[opcode_table_size].number = JNL;
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;

    /*
    jump less or equal
    jle label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JLE");
    opcode_table[opcode_table_size].number = JLE; // binary: 01111110
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;

    /*
    jump greater
    jg label2
    */
    strcpy(opcode_table[opcode_table_size].text, "JG");
    opcode_table[opcode_table_size].number = JG; // binary: 01111111
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;

    /*
    loop while 0 (aka equal)
    */
    strcpy(opcode_table[opcode_table_size].text, "LOOPZ");
    opcode_table[opcode_table_size].number = LOOPZ_LOOPE; // binary: 11100001
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    /*
    loop while not 0 (aka not equal)
    */
    strcpy(opcode_table[opcode_table_size].text, "LOOPNZ");
    opcode_table[opcode_table_size].number = LOOPNZ_LOOPNE; // binary: 11100000
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;

    /*
    'loop cx times' - i think that means the value in the cx register is
    implicitly used, we'll figure it out
    loop label2
    */
    strcpy(opcode_table[opcode_table_size].text, "LOOP");
    opcode_table[opcode_table_size].number = LOOP; // binary: 11100010
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;

    /*
    JCXZ (jump when cx is 0)
    */
    strcpy(opcode_table[opcode_table_size].text, "JCXZ");
    opcode_table[opcode_table_size].number = JCXZ; // binary: 11100011
    opcode_table[opcode_table_size].size_in_bits = 8;
    opcode_table[opcode_table_size].has_data_byte_1 = true;
    opcode_table[opcode_table_size].data_bytes_are_jump_offsets = true;
    opcode_table_size += 1;
    
    // mod '11' or 3 with its own table
    strcpy(reg_table[0][0], "AL");
    strcpy(reg_table[0][1], "CL");
    strcpy(reg_table[0][2], "DL");
    strcpy(reg_table[0][3], "BL");
    strcpy(reg_table[0][4], "AH");
    strcpy(reg_table[0][5], "CH");
    strcpy(reg_table[0][6], "DH");
    strcpy(reg_table[0][7], "BH");
    
    strcpy(reg_table[1][0], "AX");
    strcpy(reg_table[1][1], "CX");
    strcpy(reg_table[1][2], "DX");
    strcpy(reg_table[1][3], "BX");
    strcpy(reg_table[1][4], "SP");
    strcpy(reg_table[1][5], "BP");
    strcpy(reg_table[1][6], "SI");
    strcpy(reg_table[1][7], "DI");
    
    // mod '10' or 2
    strcpy(modsub3_rm_table[2][0], "BX+SI");
    strcpy(modsub3_rm_table[2][1], "BX+DI");
    strcpy(modsub3_rm_table[2][2], "BP+SI");
    strcpy(modsub3_rm_table[2][3], "BP+DI");
    strcpy(modsub3_rm_table[2][4], "SI");
    strcpy(modsub3_rm_table[2][5], "DI");
    strcpy(modsub3_rm_table[2][6], "BP");
    strcpy(modsub3_rm_table[2][7], "BX");
    
    // mod '01' or 1
    strcpy(modsub3_rm_table[1][0], "BX+SI");
    strcpy(modsub3_rm_table[1][1], "BX+DI");
    strcpy(modsub3_rm_table[1][2], "BP+SI");
    strcpy(modsub3_rm_table[1][3], "BP+DI");
    strcpy(modsub3_rm_table[1][4], "SI"); // SI + D8
    strcpy(modsub3_rm_table[1][5], "DI"); // DI + D8
    strcpy(modsub3_rm_table[1][6], "BP"); // BP + D8
    strcpy(modsub3_rm_table[1][7], "BX"); // BX + D8
    
    // mod '00' or 0
    strcpy(modsub3_rm_table[0][0], "BX+SI");
    strcpy(modsub3_rm_table[0][1], "BX+DI");
    strcpy(modsub3_rm_table[0][2], "BP+SI");
    strcpy(modsub3_rm_table[0][3], "BP+DI");
    strcpy(modsub3_rm_table[0][4], "SI");
    strcpy(modsub3_rm_table[0][5], "DI");
    strcpy(modsub3_rm_table[0][6], "DIRADDR");
    strcpy(modsub3_rm_table[0][7], "BX");
}

static uint8_t * input = NULL;
static uint32_t input_size = 0;
static uint32_t bytes_consumed = 0;
static uint32_t bits_consumed  = 0;

static uint8_t try_bits_with_offset(
    const uint32_t count,
    const uint32_t using_offset)
{
    /*
    Let's say bits_consumed is 2
    x x |
    0 1 2 3 4 5 6 7
    .. and we want 3 bits
    0 1[2 3 4]5 6 7
    
    then first we << bits_consumed (2)
    2 3 4 5 6 7 8 0
    
    next, >> (8 - count) or (8 - 3) or >> 5
    0 0 0 0 0 2 3 4
    
    next, we bitmask to be safe
    */
    uint32_t offset_bytes = using_offset / 8;
    uint32_t offset_bits = using_offset % 8;
    
    uint8_t return_value =
        (input[bytes_consumed + offset_bytes] <<
            (bits_consumed + offset_bits));
    
    return_value >>= (8 - count);
    
    uint8_t bitmask = (1 << count) - 1;
    return_value &= bitmask;
    
    return return_value;
}

static uint8_t try_bits(
    const uint32_t count)
{
    return try_bits_with_offset(
        /* const uint32_t count: */
            count,
        /* const uint32_t using_offset: */
            0);
}

static uint8_t consume_byte() {
    assert(bits_consumed == 0);
    uint8_t return_value = input[bytes_consumed];
    bytes_consumed += 1;
    
    return return_value;
}

static uint8_t consume_bits(const uint32_t count) {
    uint8_t return_value = try_bits(count);
    
    bits_consumed += count;
    
    while (bits_consumed >= 8) {
        bits_consumed -= 8;
        bytes_consumed += 1;
    }
    
    return return_value;
}

static void disassemble(
    char * recipient,
    uint32_t * good)
{
    bytes_consumed = 0;
    bits_consumed = 0;
    parsed_lines_size = 0;
    
    while (bytes_consumed < input_size) {
        if (bits_consumed != 0) {
            printf(
                "Error - bits consumed %u (not 0) at new line\n",
                bits_consumed);
            *good = false;
            return;
        }
        uint32_t bytes_consumed_at_sol = bytes_consumed;
        
        OpCode * opcode = NULL;
        uint8_t bits_to_try = 1;
        while (
            bits_to_try < 8 &&
            opcode == NULL)
        {
            bits_to_try += 1;
            
            uint32_t try_opcode = try_bits(bits_to_try);
            
            for (
                uint32_t try_i = 0;
                try_i < opcode_table_size;
                try_i++)
            {
                if (
                    opcode_table[try_i].number == try_opcode &&
                    opcode_table[try_i].size_in_bits == bits_to_try &&
                    opcode_table[try_i].text[0] != '\0')
                {
                    /*
                    hit! but this may still be a miss if there's a secondary
                    opcode
                    */
                    if (opcode_table[try_i].has_secondary_3bit_opcode) {
                        uint8_t secondary_opcode = try_bits_with_offset(
                            /* const uint32_t count: */
                                3,
                            /* const uint32_t using_offset: */
                                opcode_table[try_i].secondary_3bit_offset);
                        
                        if (
                            secondary_opcode !=
                                opcode_table[try_i].secondary_3bit_opcode)
                        {
                            continue;
                        }
                    }
                    
                    uint8_t throwaway = consume_bits(bits_to_try);
                    opcode = &opcode_table[try_i];
                    assert(opcode->number == throwaway);
                    break;
                }
            }
        }
        
        if (opcode == NULL) {
            uint32_t try_opcode = try_bits(bits_to_try);
            printf(
                "failed to find opcode: %u - ",
                try_opcode);
            print_binary(try_opcode);
            printf("\nAvailable opcodes were: ");
            for (uint32_t i = 0; i < opcode_table_size; i++) {
                if (opcode_table[i].text[0] == '\0') {
                    printf("*");
                }
                printf("%u, ", opcode_table[i].number);
            }
            *good = false;
            assert(0);
            return;
        }
        
        assert(bits_consumed < 9);
        if (bits_consumed == 8) {
            bits_consumed -= 8;
            bytes_consumed += 1;
        }
        
        // the 'd' field generally specifies the 'direction',
        // to or from register?
        // 0 means the left hand registry is the destination
        // 1 means the REG field in the second byte is the destination
        uint8_t d = opcode->hardcoded_d_field;
        if (
            opcode->has_d_field)
        {
            d = consume_bits(1);
        }

        // sign extension flag
        uint8_t s = UINT8_MAX;
        if (opcode->has_s_field) {
            s = consume_bits(1);
        }
        
        // word or byte operation? 
        // 0 = instruction operates on byte data
        // 1 = instruction operates on word data (2 bytes)
        uint8_t w = UINT8_MAX;
        if (opcode->has_w_field) {
            w = consume_bits(1);
        }
        
        // register mode / memory mode with discplacement 
        uint8_t mod = UINT8_MAX;
        if (opcode->has_mod) {
            mod = consume_bits(2);
        }

        uint8_t reg = UINT8_MAX;
        if (opcode->has_reg) {
            reg = consume_bits(3);
        }
        
        uint8_t secondary_3bit_opcode = UINT8_MAX;
        if (opcode->has_secondary_3bit_opcode) {
            secondary_3bit_opcode = consume_bits(3);
        }
        
        uint8_t r_m = UINT8_MAX;
        if (opcode->has_rm) {
            r_m = consume_bits(3);
        }
        
        uint8_t num_displacement_bytes = 0;
        char secondary_reg[10];
        secondary_reg[0] = '\0';
        uint8_t treat_secondary_reg_as_address = 0;
        
        if (opcode->has_mod) {
            switch (mod) {
                case 0: {
                    // memory mode, no displacement follows
                    if (r_m == 6) {
                        // 'except when r/m = 110, then 16 bit discplacement
                        // follows'
                        num_displacement_bytes = 2;
                        
                        // strcpy(secondary_reg, reg_table[w][reg]);
                    } else {
                        // TODO: not verified.. sure this is called '
                        // memory mode' but so are the others and they dont
                        // represent addresses. I can't find a reference to
                        // this being an address anywhere in the manual
                        treat_secondary_reg_as_address = 1;
                        strcpy(secondary_reg, modsub3_rm_table[mod][r_m]);
                    }

                    assert(num_displacement_bytes < 3);
                    break;
                }
                case 1: {
                    // memory mode, 8-bit displacement follows
                    treat_secondary_reg_as_address = true;
                    num_displacement_bytes = 1;
                    
                    strcpy(secondary_reg, modsub3_rm_table[mod][r_m]);
                    assert(secondary_reg[0] != '\0');
                    assert(num_displacement_bytes < 3);
                    break;
                }
                case 2: {
                    // memory mode, 16-bit displacement follows
                    treat_secondary_reg_as_address = true;
                    num_displacement_bytes = 2;
                    
                    strcpy(secondary_reg, modsub3_rm_table[mod][r_m]);
                    assert(num_displacement_bytes < 3);
                    break;
                }
                case 3: {
                    // register mode (no displacement)
                    assert(num_displacement_bytes == 0);
                    
                    strcpy(secondary_reg, reg_table[w][r_m]);
                    break;
                }
                default:
                    printf("Error - mod was %u, expected < 4\n", mod);
                    assert(0);
            }
        }
        assert(num_displacement_bytes < 3);
        
        uint8_t displacement_byte_1 = 0;
        uint8_t displacement_byte_2 = 0;
        int16_t displacement_bytes_combined = 0;
        
        if (num_displacement_bytes > 0) {
            displacement_byte_1 = consume_byte();
            
            if (num_displacement_bytes > 1) {
                displacement_byte_2 = consume_byte();
                
                displacement_bytes_combined =
                    (displacement_byte_2 << 8) |
                    (displacement_byte_1 & UINT8_MAX);
            } else {
                displacement_bytes_combined =
                    (int16_t)((int8_t)displacement_byte_1);
            }
        }
        
        int8_t data_bytes = 0;
        uint8_t data_byte_1 = 0;
        uint8_t data_byte_2 = 0;
        int16_t data_bytes_combined = 0;
        if (opcode->has_data_byte_1)
        {
            data_bytes = 1;
            data_byte_1 = (consume_byte() & UINT8_MAX);
            
            if (data_byte_1 < 0) {
                data_bytes_combined = INT16_MAX;
            }
            
            if (
                opcode->has_data_byte_2_always ||
                (
                opcode->has_data_byte_2_if_w &&
                    w &&
                    (!opcode->has_s_field || !s)))
            {
                data_bytes = 2;
                data_byte_2 = consume_byte();
                
                data_bytes_combined =
                    (data_byte_2 << 8) |
                    (data_byte_1 & UINT8_MAX);
            } else {
                assert(data_bytes == 1);
                data_bytes_combined = (int16_t)(int8_t)data_byte_1;
            }
        }
        
        char first_part[20];
        first_part[0] = '\0';
        char second_part[20];
        second_part[0] = '\0';
        
        strcat(parsed_lines[parsed_lines_size].parsed_text, opcode->text);
        strcat(parsed_lines[parsed_lines_size].parsed_text, " ");
        
        assert(reg <= UINT8_MAX);
        if (opcode->data_bytes_are_jump_offsets) {
        
        } else if (opcode->hardcoded_reg_w[0] != '\0') {
            assert(opcode->hardcoded_reg_b[0] != '\0');
            if (w) {
                strcat(first_part, opcode->hardcoded_reg_w);
            } else {
                strcat(first_part, opcode->hardcoded_reg_b);
            }
        } else if (opcode->data_bytes_are_immediates) {
            if (
                w &&
                opcode->has_s_field)
            {
                strcat(first_part, "word ");
            } else {
                strcat(first_part, "byte ");
            }
            strcat_int(first_part, data_bytes_combined);
        } else {
            strcat(first_part, reg_table[w][reg]);
        }
        assert(
            opcode->data_bytes_are_jump_offsets ||
            (first_part[0] != '\0'));
        
        if (secondary_reg[0] != '\0') {
            
            if (treat_secondary_reg_as_address) {
                strcat(second_part, "[");
            }
            strcat(second_part, secondary_reg);
            if (
                num_displacement_bytes > 0 &&
                displacement_bytes_combined != 0)
            {
                if (displacement_bytes_combined >= 0) {
                    strcat(second_part, "+");
                }
                strcat_int(second_part, displacement_bytes_combined);
            }
            if (treat_secondary_reg_as_address) {
                strcat(second_part, "]");
            }
        } else if (data_bytes > 0) {
            if (opcode->data_bytes_are_addresses) {
            strcat(second_part, "[");
            }
            strcat_int(second_part, data_bytes_combined);
            if (opcode->data_bytes_are_addresses) {
            strcat(second_part, "]");
            }
        } else if (num_displacement_bytes > 0) {
            strcat(second_part, "[");
            strcat_int(second_part, displacement_bytes_combined);
            strcat(second_part, "]");
        } else {
            printf("error - no data or secondary register\n");
            *good = false;
            return;
        }
        
        if (opcode->data_bytes_are_jump_offsets) {
            parsed_lines[parsed_lines_size].append_jump_bytes =
                (int32_t)data_bytes_combined;
        } else {
            assert(first_part[0] != '\0');
            assert(second_part[0] != '\0');
            if (d) {
                strcat(
                    parsed_lines[parsed_lines_size].parsed_text,
                    first_part);
                strcat(parsed_lines[parsed_lines_size].parsed_text, ", ");
                strcat(
                    parsed_lines[parsed_lines_size].parsed_text,
                    second_part);
            } else {
                strcat(
                    parsed_lines[parsed_lines_size].parsed_text,
                    second_part);
                strcat(
                    parsed_lines[parsed_lines_size].parsed_text,
                    ",");
                strcat(
                    parsed_lines[parsed_lines_size].parsed_text,
                    first_part);
            }
        }
        
        #if 0
        strcat(parsed_lines[parsed_lines_size].parsed_text, " ; opcode: ");
        strcat_binary_uint(
            parsed_lines[parsed_lines_size].parsed_text,
            opcode->number,
            opcode->size_in_bits);
        if (opcode->has_secondary_3bit_opcode) {
            strcat(parsed_lines[parsed_lines_size].parsed_text, ", opc_ext: ");
            strcat_binary_uint(
                parsed_lines[parsed_lines_size].parsed_text,
                secondary_3bit_opcode, 3);
        }
        if (opcode->has_s_field) {
            strcat(parsed_lines[parsed_lines_size].parsed_text, ", s: ");
            strcat_binary_uint(
                parsed_lines[parsed_lines_size].parsed_text,
                s,
                1);
        }
        if (opcode->has_w_field) {
            strcat(parsed_lines[parsed_lines_size].parsed_text, ", w: ");
            strcat_binary_uint(
                parsed_lines[parsed_lines_size].parsed_text,
                w,
                1);
        }
        if (opcode->has_d_field) {
            strcat(parsed_lines[parsed_lines_size].parsed_text, ", d: ");
            strcat_binary_uint(
                parsed_lines[parsed_lines_size].parsed_text,
                d,
                1);
        }
        if (opcode->has_mod) {
            strcat(parsed_lines[parsed_lines_size].parsed_text, ", mod: ");
            strcat_binary_uint(
                parsed_lines[parsed_lines_size].parsed_text,
                mod,
                2);
        }
        if (opcode->has_reg) {
            strcat(parsed_lines[parsed_lines_size].parsed_text, ", reg: ");
            strcat_binary_uint(
                parsed_lines[parsed_lines_size].parsed_text,
                reg,
                3);
        }
        if (opcode->has_rm) {
            strcat(parsed_lines[parsed_lines_size].parsed_text, ", rm: ");
            strcat_binary_uint(
                parsed_lines[parsed_lines_size].parsed_text,
                r_m,
                3);
        }
        if (num_displacement_bytes > 0) {
            strcat(parsed_lines[parsed_lines_size].parsed_text, ", disp_1: ");
            strcat_binary_uint(
                parsed_lines[parsed_lines_size].parsed_text,
                displacement_byte_1,
                8);
            if (num_displacement_bytes > 1) {
                strcat(
                    parsed_lines[parsed_lines_size].parsed_text,
                    ",
                    disp_2: ");
                strcat_binary_uint(
                    parsed_lines[parsed_lines_size].parsed_text,
                    displacement_byte_2,
                    8);
            }
            strcat(
                parsed_lines[parsed_lines_size].parsed_text,
                ",
                combined: ");
            strcat_binary_uint(
                parsed_lines[parsed_lines_size].parsed_text,
                displacement_bytes_combined >> 8,
                8);
            strcat(
                parsed_lines[parsed_lines_size].parsed_text,
                " ");
            strcat_binary_uint(
                parsed_lines[parsed_lines_size].parsed_text,
                displacement_bytes_combined & UINT8_MAX,
                8);
        }
        if (data_bytes > 0) {
            strcat(
                parsed_lines[parsed_lines_size].parsed_text,
                ",
                data_byte_1: ");
            strcat_int(
                parsed_lines[parsed_lines_size].parsed_text,
                data_byte_1);
            if (data_bytes > 1) {
                strcat(
                    parsed_lines[parsed_lines_size].parsed_text,
                    ",
                    data_byte_2: ");
                strcat_int(
                    parsed_lines[parsed_lines_size].parsed_text,
                    data_byte_2);
            }
        }
        #endif
        
        parsed_lines[parsed_lines_size].machine_bytes =
            bytes_consumed - bytes_consumed_at_sol;
        parsed_lines_size += 1;
    }
    
    *good = true;
    
    // copy our parsed output to 'recipient'
    // in this step we have to do some extra work to add labels
    recipient[0] = '\0';
    strcpy(recipient, "bits 16\n");
    
    /*
    We want to iterate through the parsed lines looking for jumps, and cache
    the exact parsed line that they need to jump to
    */
    for (uint32_t i = 0; i < parsed_lines_size; i++) {
        if (parsed_lines[i].append_jump_bytes != INT32_MIN) {
            int32_t bytes_left = parsed_lines[i].append_jump_bytes;
            int32_t direction = 1;
            if (bytes_left < 0) {
                direction = -1;
                bytes_left *= -1;
            }
            
            uint32_t target_line = i;
            while (bytes_left > 0) {
                assert(parsed_lines[target_line].machine_bytes <= bytes_left);
                bytes_left -= parsed_lines[target_line].machine_bytes;
                target_line += direction;
            }
            target_line += 1;
                        
            if (parsed_lines[target_line].label_id < 0) {
                parsed_lines[target_line].label_id = latest_label_id++;
            }
            assert(parsed_lines[target_line].label_id >= 0);
            parsed_lines[i].jump_targets_label_id =
                parsed_lines[target_line].label_id;
        }
    }
    
    for (uint32_t i = 0; i < parsed_lines_size; i++) {
        if (parsed_lines[i].label_id >= 0) {
            strcat(recipient, "label_");
            strcat_int(recipient, parsed_lines[i].label_id);
            strcat(recipient, ":\n");
        }
        strcat(recipient, parsed_lines[i].parsed_text);
        if (parsed_lines[i].jump_targets_label_id >= 0) {
            strcat(recipient, "label_");
            strcat_int(recipient, parsed_lines[i].jump_targets_label_id);
        }
        strcat(recipient, "\n");
    }
}

int main() {
    
    init_tables();
    
    #define MACHINE_CODE_CAP 10000 
    uint8_t * machine_code = (uint8_t *)malloc(MACHINE_CODE_CAP);
    machine_code[0] = '\0';
    uint32_t machine_code_size = 0;
    
    read_file(
        /* char * filename: */
            "build/machinecode",
        /* uint8_t * recipient: */
            machine_code,
        /* uint32_t * recipient_size: */
            &machine_code_size,
        /* uint32_t recipient_cap: */
            MACHINE_CODE_CAP);
    
    if (machine_code_size == 0) {
        printf("failed to read input file\n");
        return 1;
    }
    
    bytes_consumed = 0;
    bits_consumed = 0;
    
    char * recipient = (char *)malloc(2000000);
    recipient[0] = '\0';
    input = machine_code;
    input_size = machine_code_size;
    
    uint32_t success = 0;
    disassemble(
        /* char * recipient: */
            recipient,
        /* uint32_t * success: */
            &success);
    
    if (!success) {
        printf("unknown error\n");
        return 1;
    }
    
    strcat(recipient, "\n");
    printf("%s", recipient);
    return 0;
}

