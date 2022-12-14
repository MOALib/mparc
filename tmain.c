#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#define MPARC_WANT_EXTERN_AUX_UTIL_FUNCTIONS
#include "mparc.h"

/**
  * @file mparc.h
  * @author MXPSQL
  * @brief MPARC, A Dumb Archiver Format C Rewrite Of MPAR. Test file
  * @version 0.1
  * @date 2022-09-26
  * 
  * @copyright
  * 
  * Licensed To You Under Teh MIT License and the LGPL-2.1-Or-Later License
  * 
  * MIT License
  * 
  * Copyright (c) 2022 MXPSQL
  * 
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  * 
  * The above copyright notice and this permission notice shall be included in all
  * copies or substantial portions of the Software.
  * 
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  * SOFTWARE.
  * 
  * 
  * MPARC, A rewrite of MPAR IN C, a dumb archiver format
  * Copyright (C) 2022 MXPSQL
  * 
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public
  * License as published by the Free Software Foundation; either
  * version 2.1 of the License, or (at your option) any later version.
  * 
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  * 
  * You should have received a copy of the GNU Lesser General Public
  * License along with this library; if not, write to the Free Software
  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

int main(int argc, char* argv[]){
	((void)argc);
	((void)argv);
    printf("I am alive\n");

    MXPSQL_MPARC_t* archive = NULL;
    MXPSQL_MPARC_err err = MPARC_OK;
    if((err = MPARC_init(&archive)) != MPARC_OK) {
        printf("A big no happened when trying to initialize structure\n");
		MPARC_perror(err);
        printf("Init failed ok\n");
        abort();
    }
    if(archive == NULL){
		MPARC_perror(err);
        printf("Init failed ok\n");
        abort();
    }
    printf("Done initializing\n");
	char* filen[] = {/*"mparc.h", */NULL};
    for(size_t i = 0; filen[i] != NULL; i++){
        if(filen[i] == NULL) break;
    	if((err = MPARC_push_filename(archive, filen[i])) != MPARC_OK){
    	    printf("A big no happened when trying to push files\n");
    	    MPARC_perror(err);
    	    printf("File push failed\n");
    	    abort();
    	}
	}
	{
		struct pod {
			size_t i;
			char* cmd3;
		};
		struct pod* p = malloc(sizeof(struct pod));
		if(p == NULL){
			printf("Struct killed ram\n");
			abort();
		}
		p->i = 9;
		p->cmd3 = "cmd.sexe";
		if((err = MPARC_push_voidfile(archive, "structy-pod.struct", p, sizeof(*p))) != MPARC_OK){
    	    printf("A big no happened when trying to push files\n");
			MPARC_perror(err);
    	    printf("File push failed\n");
    	    abort();
    	}
		free(p);
        printf("Pushed file 1\n");
	}
	{
		if((err = MPARC_push_voidfile(archive, "mparc.struct", archive, sizeof(archive)))){
			MPARC_perror(err);
    	    printf("File push failed\n");
    	    abort();
		}
        printf("Pushed file 2\n");
	}
    {
        char* str = "strtok stinks, use strtok_r instead! Rip it from the Glibc Source!";
        if((err = MPARC_push_filestr(archive, "strtok.txt", str, strlen(str)))){
            MPARC_perror(err);
            printf("File push failed\n");
            abort();
        }
        printf("Pushed file 3\n");
    }
    /* {
        char** listprintf( NULL;
        size_t length = 0;
        MPARC_list(archive, &listout, &length);
        if(listout != NULL)
            for(size_t i = 0; i < length; i++){
                printf("%s\n", listout[i]);
            }
    } */
    /* {
        if(MPARC_exists(archive, "mparc.c") == MPARC_OK) {
            printf("File actually is in the entry.\n");
            unsigned char* bprintf( NULL;
            MPARC_peek_file(archive, "mparc.c", &bout, NULL);
            printf("%s\n", bout);
        }
        else{
            printf("File not in entry :P\n");
        }
    } */

    // test encryption
    {
        int ROTKey[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        MPARC_cipher(archive, 
        1, (unsigned char*) "yes", strlen("yes"), NULL, NULL,
        1, ROTKey, sizeof(ROTKey)/sizeof(ROTKey[0]), NULL, NULL);
        printf("Cipher set (XOR+ROT)\n");
    }

    {
        MXPSQL_MPARC_err err = MPARC_construct_filename(archive, "ck_chorder.mpar");
        if(err != MPARC_OK){
            MPARC_perror(err);
            printf("Archive construction failed%s", (err == MPARC_INTERNAL ? ": Something internal went horribly wrong" : ""));
            abort();
        }
    }
    printf("Constructed archive\n");
    // printf("%s", PRIuFAST32);
	MPARC_clear(archive);
    printf("Cleared archive\n");
    {
        MXPSQL_MPARC_err err = MPARC_parse_filename(archive, "ck_chorder.mpar");
        if(err != MPARC_OK){
			MPARC_perror(err);
            printf("Archive parsing failed\n");
            abort();
        }
        printf("Parsed file\n");
    }
	{
		char** e = NULL;
		err = MPARC_list_array(archive, &e, NULL);
        if(err != MPARC_OK){
			MPARC_perror(err);
            printf("Archive listing failed\n");
            abort();
        }
        printf("Got list from archive\n");
		for(size_t i = 0; e[i] != NULL; i++){
			printf(">%s\n", e[i]);
		}
        printf("Fully listed everything in archive\n");
	}
	/* {
		FILE* fs = fopen("mparc.struct", "wb+");
		if(fs != NULL){
			fwrite(archive, sizeof(archive), 1, fs);
			fclose(fs);
		}
	} */
	if((err =MPARC_extract(archive, ".", NULL)) != MPARC_OK){
		MPARC_sfperror(err, stderr, "DFialed to extract archive");
	}
    printf("Extracted archive\n");
    MPARC_destroy(&archive);
    printf("Destroyed archive\n");
    printf("Sizeof MXPSQL_MPARC_t: %zu\nSizeof MXPSQL_MPARC_iter_t: %zu", MPARC_MXPSQL_MPARC_t_sizeof(), MPARC_MXPSQL_MPARC_iter_t_sizeof());
    return 0;
}
