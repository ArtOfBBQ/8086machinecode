#include <stdio.h>
#include <stdint.h>
#include <assert.h>

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
    uint8_t to_cat)
{
    uint32_t i = 0;
    while (recipient[i] != '\0') {
        i++;
    }
    
    uint32_t found_leader = 0;
    
    uint32_t mod = 10000000;
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
The OPCODE is 6 bits and represents the 1st part of the assembly instruction
*/
#define MOV 34  // binary: 100010


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
The table shows the r/m for mods 'below 3' (so 00, 01, and 10)
If you are decoding the REG field, you DON'T use this table for any mod
it's indexed by the values in mod and r/m, and yields 15 chars
                            mod rm 15 chars
                             |  |  |
*/
static char modsub3_rm_table[3][8][15];

static void init_tables(void) {
    for (uint32_t w = 0; w < 2; w++) {
        for (uint32_t registry_code = 0; registry_code < 8; registry_code++) {
            reg_table[w][registry_code][0] = '\0';
        }
    }
    
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
    strcpy(modsub3_rm_table[2][0], "(BX)+(SI)+(D16)");
    strcpy(modsub3_rm_table[2][1], "1?(BX)+(SI)+(D16)");
    strcpy(modsub3_rm_table[2][2], "2?(BX)+(SI)+(D16)");
    strcpy(modsub3_rm_table[2][3], "3?(BX)+(SI)+(D16)");
    strcpy(modsub3_rm_table[2][4], "4?(BX)+(SI)+(D16)");
    strcpy(modsub3_rm_table[2][5], "5?(BX)+(SI)+(D16)");
    strcpy(modsub3_rm_table[2][6], "6?(BX)+(SI)+(D16)");
    strcpy(modsub3_rm_table[2][7], "7?(BX)+(SI)+(D16)");
    
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
    strcpy(modsub3_rm_table[0][0], "(BX)+(SI)");
    strcpy(modsub3_rm_table[0][1], "1?(BX)+(SI)");
    strcpy(modsub3_rm_table[0][2], "2?(BX)+(SI)");
    strcpy(modsub3_rm_table[0][3], "3?(BX)+(SI)");
    strcpy(modsub3_rm_table[0][4], "4?(BX)+(SI)");
    strcpy(modsub3_rm_table[0][5], "5?(BX)+(SI)");
    strcpy(modsub3_rm_table[0][6], "6?(BX)+(SI)");
    strcpy(modsub3_rm_table[0][7], "7?(BX)+(SI)");
}

static void disassemble(
    uint8_t * input,
    uint8_t input_size,
    char * recipient)
{
    strcat(recipient, "bits 16\n");
    
    uint32_t input_i = 0;
    while (input_i < input_size) {
        uint32_t bytes_consumed = 2; // we always read at least 2 bytes
        uint8_t opcode = input[input_i] >> 2;
        
        // the 'd' field generally specifies the 'direction',
        // to or from register?
        // 0 means the left hand registry is the destination
        // 1 means the REG field in the second byte is the destination
        uint8_t d = (input[input_i] >> 1) & 1; 
        
        // word or byte operation? 
        // 0 = instruction operates on byte data
        // 1 = instruction operates on word data (2 bytes)
        uint8_t w = input[input_i] & 1;
        
        // register mode / memory mode with discplacement 
        uint8_t mod = (input[input_i + 1] >> 6) & 3;
        
        // register operand (extension of opcode)
        uint8_t reg = (input[input_i + 1] >> 3) & 7;
        
        // register operand / registers to use in EA calculation
        uint8_t r_m = input[input_i + 1] & 7;
        
        switch (opcode) {
            case MOV: {
                strcat(recipient, "mov ");
                if (mod == 0) {
                    // memory mode, no displacement follows
                    
                    // note there is a 'gotcha' exception here;
                    // 'except when r/m = 110, then 16 bit discplacement
                    // follows'
                    if (r_m == 6) {

                        uint16_t extra_bytes =
                            *(uint16_t *)(&input[input_i + 2]);
                        
                        strcat(recipient, reg_table[w][reg]);
                        strcat(recipient, ", [");
                        strcat_uint(recipient, extra_bytes);
                        strcat(recipient, "]");
                        
                        bytes_consumed += 2;
                    }
                } else if (mod == 1) {
                    // memory mode, 8-bit displacement follows
                    bytes_consumed += 1;
                    
                    strcat(recipient, reg_table[w][reg]);
                    strcat(recipient, ", [");
                    strcat(recipient, modsub3_rm_table[mod][r_m]);
                    strcat(recipient, " + ");
                    strcat_uint(recipient, input[input_i + 2]);
                    strcat(recipient, "]");
                } else if (mod == 2) {
                    // memory mode, 16-bit displacement follows
                    bytes_consumed += 2;
                    
                    strcat(recipient, "mod2");
                } else if (mod == 3) {
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
                } else {
                    assert(0);
                }
                break;
            }
            default:
                strcat(recipient, "???");
                break;
        }
        
        strcat(recipient, "\n");
        input_i += bytes_consumed;
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
    
    char recipient[256];
    recipient[0] = '\0';
    disassemble(
        /* uint8_t * input: */
            machine_code,
        /* uint8_t input_size: */
            machine_code_size,
        /* char * recipient: */
            recipient);
    
    printf("output:\n%s\n", recipient);
}

