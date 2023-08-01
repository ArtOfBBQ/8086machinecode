#include <stdio.h>
#include <stdint.h>

// MOV is 100010
//       32 + 2 = 34
#define MOV 34

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

/*
static void strcat_uint(
    char * recipient,
    uint8_t to_cat)
{
    uint32_t i = 0;
    while (recipient[i] != '\0') {
        i++;
    }
    
    recipient[i++] = '0' + to_cat;
    
    recipient[i] = '\0';
}
*/

#define AL 0
#define CL 1
#define DL 2
#define BL 3
#define AH 4
#define CH 5
#define DH 6
#define BH 7

#define AX 0
#define CX 1
#define DX 2
#define BX 3
#define SP 4
#define BP 5
#define SI 6
#define DI 7

static char register_table[2][8][3];
static void init_tables(void) {
    for (uint32_t w = 0; w < 2; w++) {
        for (uint32_t registry_code = 0; registry_code < 8; registry_code++) {
            register_table[w][registry_code][0] = '\0';
        }
    }
    
    strcat(register_table[0][AL], "AL");
    strcat(register_table[0][CL], "CL");
    strcat(register_table[0][DL], "DL");
    strcat(register_table[0][BL], "BL");
    strcat(register_table[0][AH], "AH");
    strcat(register_table[0][CH], "CH");
    strcat(register_table[0][DH], "DH");
    strcat(register_table[0][BH], "BH");
    
    strcat(register_table[1][AX], "AX");
    strcat(register_table[1][CX], "CX");
    strcat(register_table[1][DX], "DX");
    strcat(register_table[1][BX], "BX");
    strcat(register_table[1][SP], "SP");
    strcat(register_table[1][BP], "BP");
    strcat(register_table[1][SI], "SI");
    strcat(register_table[1][DI], "DI");
}

static void disassemble(
    uint8_t * input,
    uint8_t input_size,
    char * recipient)
{
    strcat(recipient, "bits 16\n");
    
    for (uint32_t i = 0; (i+1) < input_size; i += 2) {
        uint8_t opcode = input[i] >> 2;
        
        // the 'd' field generally specifies the 'direction',
        // to or from register?
        // 1 means the REG field in the second byte is the destination
        uint8_t d = (input[i] >> 1) & 1; 
        
        // word or byte operation? 
        // 0 = instruction operates on byte data
        // 1 = instruction operates on word data (2 bytes)
        uint8_t w = input[i] & 1;
        
        // register mode / memory mode with discplacement 
        // TODO: use this
        // uint8_t mod = (input[i + 1] >> 6) & 3;
        
        // register operand (extension of opcode)
        uint8_t reg = (input[i + 1] >> 3) & 7;
        
        // register operand / registers to use in EA calculation
        uint8_t r_m = input[i + 1] & 7;
        
        switch (opcode) {
            case MOV: {
                strcat(recipient, "mov ");

                if (d) {
                    strcat(recipient, register_table[w][reg]);
                    strcat(recipient, ", ");
                    strcat(recipient, register_table[w][r_m]);
                } else {
                    strcat(recipient, register_table[w][r_m]);
                    strcat(recipient, ", ");
                    strcat(recipient, register_table[w][reg]);
                }
                
                break;
            }
            default:
                strcat(recipient, "???");
                break;
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

