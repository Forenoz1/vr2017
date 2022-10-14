/* Name: Haoyuan He
 * unikey: hahe0181
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Step 1. Check the validation of command-line arguments.
    if (argc < 5) {
        printf("Error: Not enough command line arguments.\n");
        exit(1);
    }
    if (argc > 5) {
        printf("Error: Too many command line arguments.\n");
        exit(1);
    }

    if (access(argv[1], 0) != 0) {
        printf("Error: File %s does not exist!\n", argv[1]);
        exit(1);
    }

    // Step 2. Print the delimiter and parity byte.
    unsigned char deli_value[3];
    for (int i = 2; i < 5; i++) {
        char *arg_ptr = argv[i];
        char *ptr;
        int temp = strtol(argv[i], &ptr, 16);
        if (strlen(argv[i]) != 4) {
            printf("Error: Argument for delimiter byte %d is not of the correct length\n", i - 2);
            exit(1);
        } else if (*arg_ptr != '0' || (*(arg_ptr + 1) != 'x' && *(arg_ptr + 1) != 'X')) {
            printf("Error: Argument for delimiter byte %d does not begin with 0x\n", i - 2);
            exit(1);
        } else if (strlen(ptr) != 0) {
            printf("Error: Argument for delimiter byte %d is not a valid hex value\n", i - 2);
            exit(1);
        } else {
            deli_value[i - 2] = (unsigned char)temp;
        }
    }

    for (int i = 0; i < 3; i++) {
        printf("Delimiter byte %d is: %d\n", i, deli_value[i]);
    }

    unsigned char checksum = deli_value[0] ^ deli_value[1] ^ deli_value[2];
    printf("Checksum is: %d\n", checksum);

    // Step 3. Open the file, get the length of file, and process special cases(length <= 4).
    FILE *file = fopen(argv[1], "rb");
    if (file == NULL) {
        printf("Fail to open the file.\n");
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    // Special case: the file is empty, so print nothing and exit.
    if (length == 0) {
        printf("\n");
        exit(1);
    } else if (length < 4) {        // Special case: the length of the file is less than 4, means no trailer.
        printf("\nChunk: 0 at offset: 0\n");
        printf("Error: the size of the chunk is not a multiple of 5.\n");
        exit(1);
    }
    rewind(file);

    // Step 4. Read one byte each iteration until reach EOF. If there is a trailer, process the chunk before it.
    unsigned char trailer_pattern[4];
    memcpy(trailer_pattern, deli_value, 3);
    trailer_pattern[3] = checksum;
    int cur_byte;
    unsigned char trailer_cand[4];  // For trailer match with trailer_pattern[]
    unsigned char chunk[640];       // 640 because a valid chunk never exceeds 640 bytes. Only need 640 for future use.
    int prev_trailer_offset_e = 0;
    int chunk_size = 0;
    int chunk_num = 0;
    int chunk_exceed = 0;
    while ((cur_byte = fgetc(file)) != EOF) {
        // 1. Read 4 bytes for trailer match and change the position for next iteration.
        fseek(file, -1, SEEK_CUR);
        int cur_offset = ftell(file);
        if (fread(trailer_cand, 4, 1, file)) {
            fseek(file, -3, SEEK_CUR);
        } else {
            fseek(file, cur_offset + 1, SEEK_SET);
        }

        // 2. Determine whether current location is a trailer. If yes, process the chunk before this trailer. If no,
        // store the byte on current location in chunk[].
        if ((trailer_pattern[0] == trailer_cand[0] && trailer_pattern[1] == trailer_cand[1]
        && trailer_pattern[2] == trailer_cand[2] && trailer_pattern[3] == trailer_cand[3]) || cur_offset == length - 1) {
            // Determine whether reach the end of the file.
            if (cur_offset == length - 1) {
                if (chunk_size < 640) {
                    chunk[chunk_size] = (unsigned char)cur_byte;
                    chunk_size++;
                    cur_offset++;
                } else {
                    chunk_exceed = 1;
                }
            }

            // Print Chunk No. and its offset.
            printf("\nChunk: %d at offset: %d\n", chunk_num, prev_trailer_offset_e);
            int chunk_sum_x = 0;
            int chunk_sum_y = 0;
            int chunk_sum_z = 0;
            float valid_packet_num = 0;

            // Whether chunk exceeds 640 bytes. If yes, reset chunk size and update the trailer position and file position.
            if (chunk_exceed) {
                printf("Error: Chunk size exceeds the maximum allowable chunk size of 640 bytes.\n");
                chunk_size = 0;
                chunk_num++;
                prev_trailer_offset_e = cur_offset + 4;
                fseek(file, 3, SEEK_CUR);
                continue;
            } else if ((cur_offset - prev_trailer_offset_e) == 0) {     // Whether a valid packet exists.
                valid_packet_num = 0;
            } else if ((cur_offset - prev_trailer_offset_e) % 5 != 0) {     // Whether chunk size is divisible by 5.
                printf("Error: Chunk must be divisible by 5 bytes.\n");
                chunk_size = 0;
                chunk_num++;
                prev_trailer_offset_e = cur_offset + 4;
                fseek(file, 3, SEEK_CUR);
                continue;
            } else {        // A valid chunk and then can process packets in it.
                // process packets
                int packet_num = chunk_size / 5;
                unsigned char prev_x, prev_y, prev_z;
                for (int i = 0; i < packet_num; i++) {
                    printf("    Packet: %d\n", i);
                    unsigned char b0 = chunk[5 * i];
                    unsigned char b1 = chunk[5 * i + 1];
                    unsigned char b2 = chunk[5 * i + 2];
                    unsigned char swizzle = chunk[5 * i + 3];
                    unsigned char parity = chunk[5 * i + 4];

                    unsigned char check = b0 ^ b1 ^ b2 ^ swizzle;
                    if (check != parity) {
                        printf("        Ignoring packet. Checksum was: %d instead of %d.\n", check, parity);
                        continue;
                    }

                    if (swizzle < 1 || swizzle > 6) {
                        printf("        Ignoring packet. Swizzle byte was: %d but can only be between 1 and 6.\n", swizzle);
                        continue;
                    }

                    unsigned char x, y, z;
                    printf("        Data before swizzle -> B0: %d, B1: %d, B2: %d\n", b0, b1, b2);
                    switch (swizzle) {
                        case 1:
                            printf("        Swizzle: XYZ\n");
                            x = b0;
                            y = b1;
                            z = b2;
                            break;
                        case 2:
                            printf("        Swizzle: XZY\n");
                            x = b0;
                            z = b1;
                            y = b2;
                            break;
                        case 3:
                            printf("        Swizzle: YXZ\n");
                            y = b0;
                            x = b1;
                            z = b2;
                            break;
                        case 4:
                            printf("        Swizzle: YZX\n");
                            y = b0;
                            z = b1;
                            x = b2;
                            break;
                        case 5:
                            printf("        Swizzle: ZXY\n");
                            z = b0;
                            x = b1;
                            y = b2;
                            break;
                        case 6:
                            printf("        Swizzle: ZYX\n");
                            z = b0;
                            y = b1;
                            x = b2;
                            break;
                    }

                    printf("        Data after swizzle -> X: %d, Y: %d, Z: %d\n", x, y, z);

                    if (i != 0) {
                        if (abs(prev_x - x) > 25) {
                            printf("        Ignoring packet. X: %d. Previous valid packet's X: %d. %d > 25.\n", x,
                                   prev_x, abs(prev_x - x));
                            continue;
                        }
                        if (abs(prev_y - y) > 25) {
                            printf("        Ignoring packet. Y: %d. Previous valid packet's Y: %d. %d > 25.\n", y,
                                   prev_y, abs(prev_y - y));
                            continue;
                        }
                        if (abs(prev_z - z) > 25) {
                            printf("        Ignoring packet. Z: %d. Previous valid packet's Z: %d. %d > 25.\n", z,
                                   prev_z, abs(prev_z - z));
                            continue;
                        }
                    }

                    prev_x = x;
                    prev_y = y;
                    prev_z = z;

                    valid_packet_num++;
                    chunk_sum_x += x;
                    chunk_sum_y += y;
                    chunk_sum_z += z;
                }
            }

            // Whether a valid packet exists. If not, reset chunk size and update the file position.
            if (valid_packet_num == 0) {
                printf("    No valid packets were found for this chunk.\n");
                chunk_size = 0;
                chunk_num++;
                prev_trailer_offset_e = cur_offset + 4;
                fseek(file, 3, SEEK_CUR);
                continue;
            } else {
                printf("    Chunk Average X: %.2f, Average Y: %.2f, Average Z: %.2f\n", chunk_sum_x / valid_packet_num,
                       chunk_sum_y / valid_packet_num, chunk_sum_z / valid_packet_num);
            }
            chunk_size = 0;
            chunk_num++;
            prev_trailer_offset_e = cur_offset + 4;
            fseek(file, 3, SEEK_CUR);
        } else {        // Chunk size is over 640 bytes. Signal the chunk_exceed to 1 and reset chunk size.
            if (chunk_size > 639) {
                chunk_exceed = 1;
                chunk_size = 0;
            } else {        // Store the current byte to the chunk.
                chunk[chunk_size] = (unsigned char)cur_byte;
                chunk_size++;
            }
        }
    }

    fclose(file);
    printf("\n");
    return 0;
}

