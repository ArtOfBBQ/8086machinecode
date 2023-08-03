#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

// static void print_binary(
//     const uint8_t input)
// {
//     for (int32_t i = 7; i >= 0; i--) {
//         printf("%u", (input >> i) & 1);
//     }
// }

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
From 2 bytes of machine code, we will get an opcode, a 'W' flag, a 'D' flag,
a 'mod' field, a 'reg' field and an 'r_m' field.
More bytes may follow depending on what we get.

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
#define MOV_REGMEMTOREG 34  // binary: 100010
#define MOV_IMMTOREG    11  // binary:   1011
typedef struct OpCode {
    char text[4];
    uint8_t number;
    uint8_t has_d_field;
    uint8_t has_w_field;
    uint8_t has_mod;
    uint8_t has_reg;
    uint8_t has_rm;
} OpCode;
#define OPCODE_TABLE_SIZE 2
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
        opcode_table[i].has_d_field = false;
        opcode_table[i].has_w_field = false;
        opcode_table[i].has_mod = false;
        opcode_table[i].has_reg = false;
        opcode_table[i].has_rm = false;
    }
    
    strcpy(opcode_table[opcode_table_size].text, "MOV");
    opcode_table[opcode_table_size].number = MOV_IMMTOREG; // 1101
    opcode_table[opcode_table_size].has_d_field = false;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_mod = false;
    opcode_table[opcode_table_size].has_reg = true;
    opcode_table[opcode_table_size].has_rm = false;
    opcode_table_size += 1;
    
    strcpy(opcode_table[opcode_table_size].text, "MOV");
    opcode_table[opcode_table_size].number = MOV_REGMEMTOREG; // 100010
    opcode_table[opcode_table_size].has_d_field = true;
    opcode_table[opcode_table_size].has_w_field = true;
    opcode_table[opcode_table_size].has_mod = true;
    opcode_table[opcode_table_size].has_reg = true;
    opcode_table[opcode_table_size].has_rm = true;
    opcode_table_size += 1;
    
    assert(opcode_table[0].number > 0);
    assert(opcode_table[1].number > 0);
    
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
    strcpy(modsub3_rm_table[2][0], "BX + SI");
    strcpy(modsub3_rm_table[2][1], "BX + DI");
    strcpy(modsub3_rm_table[2][2], "BP + SI");
    strcpy(modsub3_rm_table[2][3], "BP + DI");
    strcpy(modsub3_rm_table[2][4], "SI");
    strcpy(modsub3_rm_table[2][5], "DI");
    strcpy(modsub3_rm_table[2][6], "BP");
    strcpy(modsub3_rm_table[2][7], "BX");
    
    // mod '01' or 1
    strcpy(modsub3_rm_table[1][0], "BX + SI");
    strcpy(modsub3_rm_table[1][1], "BX + DI");
    strcpy(modsub3_rm_table[1][2], "BP + SI");
    strcpy(modsub3_rm_table[1][3], "BP + DI");
    strcpy(modsub3_rm_table[1][4], "SI"); // SI + D8
    strcpy(modsub3_rm_table[1][5], "DI"); // DI + D8
    strcpy(modsub3_rm_table[1][6], "BP"); // BP + D8
    strcpy(modsub3_rm_table[1][7], "BX"); // BX + D8
    
    // mod '00' or 0
    strcpy(modsub3_rm_table[0][0], "BX + SI");
    strcpy(modsub3_rm_table[0][1], "BX + DI");
    strcpy(modsub3_rm_table[0][2], "BP + SI");
    strcpy(modsub3_rm_table[0][3], "BP + DI");
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
    char * recipient)
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
        }
        // bits_consumed = 3;
        
        OpCode * opcode = NULL;
        uint8_t bits_to_try = 1;
        while (
            bits_to_try < 9 &&
            opcode == NULL)
        {
            uint32_t try_opcode = try_bits(bits_to_try);
            
            for (
                uint32_t try_i = 0;
                try_i < opcode_table_size;
                try_i++)
            {
                if (
                    opcode_table[try_i].number == try_opcode &&
                    opcode_table[try_i].text[0] != '\0')
                {
                    uint8_t throwaway = consume_bits(bits_to_try);
                    opcode = &opcode_table[try_i];
                    assert(opcode->number == throwaway);
                    break;
                }
            }
            bits_to_try += 1;
        }
        
        if (opcode == NULL) {
            printf("%s\n", recipient);
            printf(
                "failed to find opcode: %u\n",
                input[bytes_consumed]);
            assert(0);
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
        uint8_t d = UINT8_MAX;
        if (opcode->has_d_field) {
            d = consume_bits(1);
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
        uint8_t reg = UINT8_MAX;
        uint8_t r_m = UINT8_MAX;
        uint8_t extra_bytes = 0;
        uint8_t extra_byte_1 = UINT8_MAX;
        uint8_t extra_byte_2 = UINT8_MAX;
        
        if (opcode->has_mod) {
            mod = consume_bits(2);
        }
        
        if (opcode->has_reg) {
            reg = consume_bits(3);
        }

        if (opcode->has_rm) {
            r_m = consume_bits(3);
        }
        
        strcat(recipient, opcode->text);
        strcat(recipient, " ");
        
        if (opcode->has_mod) {
            switch (mod) {
                case 0: {
                    // memory mode, no displacement follows
                    
                    // TODO: this has 2 discplacement bytes, but im only using
                    // 1, definitely wrong... probably getting the right result
                    // because number is <= UINT8_MAX, let's check later
                    
                    // note there is a 'gotcha' exception here;
                    // 'except when r/m = 110, then 16 bit discplacement
                    // follows'
                    if (r_m == 6) {
                        extra_bytes = 2;
                        extra_byte_1 = consume_byte();
                        extra_byte_2 = consume_byte();

                        uint16_t extra_bytes_combined =
                            (extra_byte_2 << 8) + extra_byte_1;
                        
                        if (d) {
                            strcat(recipient, reg_table[w][reg]);
                            strcat(recipient, ", [");
                            strcat_uint(recipient, extra_bytes_combined);
                            strcat(recipient, "]");
                        } else {
                            strcat(recipient, "[");
                            strcat_uint(recipient, extra_bytes_combined);
                            strcat(recipient, "], ");
                            strcat(recipient, reg_table[w][reg]);
                        }
                    } else {
                        if (d) {
                            strcat(recipient, reg_table[w][reg]);
                            strcat(recipient, ", [");
                            strcat(recipient, modsub3_rm_table[mod][r_m]);
                            strcat(recipient, "]");
                        } else {
                            strcat(recipient, "[");
                            strcat(recipient, modsub3_rm_table[mod][r_m]);
                            strcat(recipient, "], ");
                            strcat(recipient, reg_table[w][reg]);
                        }
                    }
                    break;
                }
                case 1: {
                    // memory mode, 8-bit displacement follows
                    extra_bytes = 1;
                    assert(d != UINT8_MAX);
                    assert(r_m != UINT8_MAX);
                    extra_byte_1 = consume_byte();
                    
                    if (d) {
                        strcat(recipient, reg_table[w][reg]);
                        strcat(recipient, ", [");
                        strcat(recipient, modsub3_rm_table[mod][r_m]);
                        if (extra_byte_1 > 0) {
                            strcat(recipient, " + ");
                            strcat_uint(recipient, extra_byte_1);
                        }
                        strcat(recipient, "]");
                    } else {
                        strcat(recipient, "[");
                        strcat(recipient, modsub3_rm_table[mod][r_m]);
                        if (extra_byte_1 > 0) {
                            strcat(recipient, " + ");
                            strcat_uint(recipient, extra_byte_1);
                        }
                        strcat(recipient, "], ");
                        strcat(recipient, reg_table[w][reg]);
                    }
                    break;
                }
                case 2: {
                    // memory mode, 16-bit displacement follows
                    extra_bytes = 2;
                    assert(d != UINT8_MAX);
                    assert(r_m != UINT8_MAX);
                    extra_byte_1 = consume_byte();
                    extra_byte_2 = consume_byte();
                    uint16_t extra_bytes_combined =
                        (extra_byte_2 << 8) + extra_byte_1;
                    
                    if (d) {
                        strcat(recipient, reg_table[w][reg]);
                        strcat(recipient, ", [");
                        strcat(recipient, modsub3_rm_table[mod][r_m]);
                        strcat(recipient, " + ");
                        strcat_uint(recipient, extra_bytes_combined);
                        strcat(recipient, "]");
                    } else {
                        strcat(recipient, "[");
                        strcat(recipient, modsub3_rm_table[mod][r_m]);
                        strcat(recipient, " + ");
                        strcat_uint(recipient, extra_bytes_combined);
                        strcat(recipient, "], ");
                        strcat(recipient, reg_table[w][reg]);
                    }
                    break;
                }
                case 3: {
                    // register mode (no displacement)
                    if (d) {
                        strcat(recipient, reg_table[w][reg]);
                        strcat(recipient, ", ");
                        strcat(recipient, reg_table[w][r_m]);
                    } else {
                        strcat(recipient, reg_table[w][r_m]);
                        strcat(recipient, ", ");
                        strcat(recipient, reg_table[w][reg]);
                    }
                    break;
                }
                default:
                    printf("Error - mod was %u, expected < 4\n", mod);
                    assert(0);
            }
        } else {
            extra_bytes = 1 + w;
            extra_byte_1 = consume_byte();
            if (extra_bytes == 1) {
                strcat(recipient, reg_table[w][reg]);
                strcat(recipient, ", ");
                strcat_uint(recipient, extra_byte_1);
            } else {
                assert(extra_bytes == 2);
                extra_byte_2 = consume_byte();
                // manual: 'the second byte is always most significant'
                int16_t extra_bytes_combined =
                    (extra_byte_2 << 8) + extra_byte_1;
                strcat(recipient, reg_table[w][reg]);
                strcat(recipient, ", ");
                strcat_int(recipient, extra_bytes_combined);
            }
        }
        
        strcat(recipient, "\n");
    }
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
        return 0;
    } else {
        printf("Read input file (%u bytes)\n", machine_code_size);
    }
    
    bytes_consumed = 0;
    bits_consumed = 0;
    
    char recipient[1024];
    recipient[0] = '\0';
    input = machine_code;
    input_size = machine_code_size;
    disassemble(
        /* char * recipient: */
            recipient);
    
    printf("output:\n%s\n", recipient);
}

