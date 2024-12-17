#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tee_client_api.h>
#include <TEEencrypt_ta.h>

#define MAX_TEXT_LEN 1024
#define RSA_BUFFER_SIZE 128

//파일 읽기
void read_file(const char *filename, char *buffer) {
    FILE *file = fopen(filename, "r");
    if (!file)
        errx(1, "Failed to open %s", filename);
    fread(buffer, 1, MAX_TEXT_LEN, file);
    fclose(file);
}

//파일 쓰기
void write_file(const char *filename, const char *data) {
    FILE *file = fopen(filename, "w");
    if (!file)
        errx(1, "Failed to write to %s", filename);
    fprintf(file, "%s", data);
    fclose(file);
}


int main(int argc, char *argv[]) {

    //들어오는 인자가 올바르지 않을 경우
    //argv[0]: ./TEEencrypt 
    //argv[1]: -e or -d
    //argv[2]: Text File
    //argv[3]: Algorithm or Key File
    if (argc != 4) {
        printf("Usage: TEEencrypt -e [plaintext file] [caesar or rsa] (encryption)\n");
        printf("       TEEencrypt -d [ciphertext file] [key file] (decryption)\n");
        return 1;
    }

    TEEC_Result res;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = TA_TEEencrypt_UUID;
    uint32_t err_origin;

    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

    res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_OpenSession failed with code 0x%x origin 0x%x", res, err_origin);

    memset(&op, 0, sizeof(op));

    if (strcmp(argv[1], "-e") == 0) {
        char plaintext[MAX_TEXT_LEN] = {0};
        char ciphertext[MAX_TEXT_LEN] = {0};

        read_file(argv[2], plaintext);

        if (strcmp(argv[3], "caesar") == 0) {
        printf("Using Caesar cipher for encryption...\n");
            op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT, TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE);
            op.params[0].tmpref.buffer = plaintext;
            op.params[0].tmpref.size = strlen(plaintext) + 1;

            res = TEEC_InvokeCommand(&sess, TA_TEEencrypt_CMD_ENC_VALUE, &op, &err_origin);
            if (res != TEEC_SUCCESS)
                errx(1, "TEEC_InvokeCommand (encrypt) failed 0x%x origin 0x%x", res, err_origin);

        snprintf(ciphertext, MAX_TEXT_LEN, "%s", (char *)op.params[0].tmpref.buffer);
        write_file("ciphertext.txt", ciphertext);

        char encrypted_key[16];
            snprintf(encrypted_key, sizeof(encrypted_key), "%d", op.params[1].value.a);
            write_file("encryptedkey.txt", encrypted_key);

            printf("Caesar Encryption complete. Output: ciphertext.txt, encryptedkey.txt\n");

        } else if (strcmp(argv[3], "rsa") == 0) {
            printf("Using RSA encryption...\n");
            op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE);
            op.params[0].tmpref.buffer = plaintext;
            op.params[0].tmpref.size = strlen(plaintext) + 1;

            char rsa_ciphertext[RSA_BUFFER_SIZE] = {0};
            op.params[1].tmpref.buffer = rsa_ciphertext;
            op.params[1].tmpref.size = sizeof(rsa_ciphertext);

            res = TEEC_InvokeCommand(&sess, TA_TEEencrypt_CMD_RSA_KEY_ENC, &op, &err_origin);
            if (res != TEEC_SUCCESS)
                errx(1, "TEEC_InvokeCommand (RSA encrypt) failed 0x%x origin 0x%x", res, err_origin);

            write_file("ciphertext_rsa.txt", rsa_ciphertext);
            printf("RSA Encryption complete. Output: ciphertext_rsa.txt\n");

        } else {
            printf("Invalid algorithm. Use 'caesar' or 'rsa'.\n");
            return 1;
        }

    } else if (strcmp(argv[1], "-d") == 0) {
        if (argc != 4) {
            printf("Decryption requires both ciphertext and key files.\n");
            return 1;
        }

        char ciphertext[MAX_TEXT_LEN] = {0};
        char plaintext[MAX_TEXT_LEN] = {0};
        int encrypted_key;

        read_file(argv[2], ciphertext);
        FILE *key_file = fopen(argv[3], "r");
        if (!key_file)
            errx(1, "Failed to open %s", argv[3]);
        fscanf(key_file, "%d", &encrypted_key);
        fclose(key_file);

        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT, TEEC_VALUE_INPUT, TEEC_NONE, TEEC_NONE);
        op.params[0].tmpref.buffer = ciphertext;
        op.params[0].tmpref.size = strlen(ciphertext) + 1;
        op.params[1].value.a = encrypted_key;

        res = TEEC_InvokeCommand(&sess, TA_TEEencrypt_CMD_DEC_VALUE, &op, &err_origin);
        if (res != TEEC_SUCCESS)
            errx(1, "TEEC_InvokeCommand (decrypt) failed 0x%x origin 0x%x", res, err_origin);

        snprintf(plaintext, MAX_TEXT_LEN, "%s", (char *)op.params[0].tmpref.buffer);
        write_file("plaintext.txt", plaintext);

        printf("Decryption complete. Output: plaintext.txt\n");
        } else {
            printf("Invalid option. Use -e (encryption) or -d (decryption).\n");
        }

        TEEC_CloseSession(&sess);
        TEEC_FinalizeContext(&ctx);
        return 0;
}
