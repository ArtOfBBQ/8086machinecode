#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

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
        
        *recipient_size = elements_read;
        
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
#define MOV_MEMTOACC           80 // binary: 1010000
#define MOV_ACCTOMEM           81 // binary: 1010001

#define ADD_REGMEMTOREG         0 // binary: 000000
#define ADD_IMMTOREGMEM        32 // binary: 100000
#define ADD_IMMTOACC            2 // binary: 0000010
typedef struct OpCode {
    char text[4];
    uint8_t number;
    uint8_t size_in_bits;
    uint8_t has_d_field;
    uint8_t hardcoded_d_field; // this opcode always behaves as if d = x
    uint8_t has_s_field;
    uint8_t has_w_field;
    uint8_t has_mod;
    uint8_t has_triple_mistery_bits;
    uint8_t has_reg;
    char hardcoded_reg[3]; // this opcode always behaves as if reg = "xx"
    uint8_t has_rm;
    uint8_t extra_bytes_are_addresses;
    uint8_t has_data_byte_1;
    uint8_t has_data_byte_2_if_w;
    uint8_t has_data_byte_2_always;
} OpCode;
#define OPCODE_TABLE_SIZE 20
static OpCode opcode_table[OPCODE_TABLE_SIZE];
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
    
    for (uint32_t i = 0; i < OPCODE_TABLE_SIZE; i++) {
        opcode_table[i].text[0] = '\0';
        opcode_table[i].size_in_bits = 0;
        opcode_table[i].has_d_field = false;
        opcode_table[i].hardcoded_d_field = 0;
        opcode_table[i].has_s_field = false;
        opcode_table[i].has_w_field = false;
        opcode_table[i].has_mod = false;
        opcode_table[i].has_reg = false;
        opcode_table[i].hardcoded_reg[0] = '\0';
        opcode_table[i].has_triple_mistery_bits = false;
        opcode_table[i].has_rm = false;
        opcode_table[i].has_data_byte_1 = false;
        opcode_table[i].has_data_byte_2_if_w = false;
        opcode_table[i].has_data_byte_2_always = false;
        opcode_table[i].extra_bytes_are_addresses = false;
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
static uint8_t input_size = 0;
static uint32_t bytes_consumed = 0;
static uint32_t bits_consumed  = 0;

static uint8_t try_bits(const uint32_t count) {
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
    uint8_t return_value = (input[bytes_consumed] << bits_consumed);
    
    return_value >>= (8 - count);
    
    uint8_t bitmask = (1 << count) - 1;
    return_value &= bitmask;
    
    return return_value;
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
    strcat(recipient, "bits 16\n");
    
    bytes_consumed = 0;
    bits_consumed = 0;
    
    while (bytes_consumed < input_size) {
        if (bits_consumed != 0) {
            printf("%s\n", recipient);
            printf(
                "Error - bits consumed %u (not 0) at new line\n",
                bits_consumed);
            *good = false;
            return;
        }
        
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
                    uint8_t throwaway = consume_bits(bits_to_try);
                    opcode = &opcode_table[try_i];
                    assert(opcode->number == throwaway);
                    break;
                }
            }
        }
        
        if (opcode == NULL) {
            printf("%s\n", recipient);
            uint32_t try_opcode = try_bits(bits_to_try);
            printf(
                "failed to find opcode: %u (",
                try_opcode);
            print_binary(try_opcode);
            printf(")\n");
            *good = false;
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
        
        uint8_t triple_mistery_bits = UINT8_MAX;
        if (opcode->has_triple_mistery_bits) {
            triple_mistery_bits = consume_bits(3);
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
        
        int8_t displacement_byte_1 = 0;
        int8_t displacement_byte_2 = 0;
        if (num_displacement_bytes > 0) {
            displacement_byte_1 = consume_byte();
        }
        if (num_displacement_bytes > 1) {
            displacement_byte_2 = consume_byte();
        }
        int8_t displacement_bytes_combined =
            (displacement_byte_2 << 8) + displacement_byte_1;
        
        int8_t data_bytes = 0;
        int8_t data_byte_1 = 0;
        int8_t data_byte_2 = 0;
        if (opcode->has_data_byte_1) {
            data_bytes = 1;
            data_byte_1 = consume_byte();
            
            if (
                opcode->has_data_byte_2_always ||
                (opcode->has_data_byte_2_if_w && w))
            {
                data_bytes = 2;
                data_byte_2 = consume_byte();
            }
        }
        int16_t data_bytes_combined =
            (data_byte_2 << 8) + data_byte_1;
        
        char first_part[20];
        first_part[0] = '\0';
        char second_part[20];
        second_part[0] = '\0';
        
        strcat(recipient, opcode->text);
        strcat(recipient, " ");
       
        assert(reg < UINT8_MAX); 
        strcat(first_part, reg_table[w][reg]);
        assert(first_part[0] != '\0');
        
        if (secondary_reg[0] != '\0') {
            if (treat_secondary_reg_as_address) {
                strcat(second_part, "[");
            }
            strcat(second_part, secondary_reg);
            if (num_displacement_bytes > 0) {
                strcat(second_part, "+");
                strcat_int(second_part, displacement_bytes_combined);
            }
            if (treat_secondary_reg_as_address) {
                strcat(second_part, "]");
            }
        } else if (data_bytes > 0) {
            strcat_int(second_part, data_bytes_combined);
        } else if (num_displacement_bytes > 0) {
            strcat(second_part, "[");
            strcat_int(second_part, displacement_bytes_combined);
            strcat(second_part, "]");
        } else {
            printf("error - no data or secondary register\n");
            *good = false;
            return;
        }
        
        assert(first_part[0] != '\0');
        assert(second_part[0] != '\0');
        if (d) {
            strcat(recipient, first_part);
            strcat(recipient, ", ");
            strcat(recipient, second_part);
        } else {
            strcat(recipient, second_part);
            strcat(recipient, ", ");
            strcat(recipient, first_part);
        }
        
        #if 0
        strcat(recipient, " ; opcode: ");
        strcat_binary_uint(
            recipient,
            opcode->number,
            opcode->size_in_bits);
        strcat(recipient, ", w: ");
        strcat_binary_uint(recipient, w, 1);
        strcat(recipient, ", d: ");
        strcat_binary_uint(recipient, d, 1);
        strcat(recipient, ", mod: ");
        strcat_binary_uint(recipient, mod, 2);
        strcat(recipient, ", reg: ");
        strcat_binary_uint(recipient, reg, 3);
        strcat(recipient, ", rm: ");
        strcat_binary_uint(recipient, r_m, 3);
        strcat(recipient, ", num_displacement_bytes: ");
        strcat_int(recipient, num_displacement_bytes);
        #endif
        
        strcat(recipient, "\n");
    }

    *good = true;
}

int main() {
    
    init_tables();
    
    uint8_t machine_code[128];
    uint32_t machine_code_size = 0;
    
    read_file(
        /* char * filename: */
            "build/machinecode",
        /* uint8_t * recipient: */
            &machine_code[0],
        /* uint32_t * recipient_size: */
            &machine_code_size,
        /* uint32_t recipient_cap: */
            128);
    
    if (machine_code_size == 0) {
        printf("failed to read input file\n");
        return 1;
    }
    
    bytes_consumed = 0;
    bits_consumed = 0;
    
    char recipient[2048];
    recipient[0] = '\0';
    input = machine_code;
    input_size = machine_code_size;

    uint32_t success = 0;
    disassemble(
        /* char * recipient: */
            recipient,
        &success);
    
    if (!success) { return 1; }
    printf("%s", recipient);
    return 0;
}

