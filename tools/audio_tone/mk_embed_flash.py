#!/user/bin/env python

#  ESPRESSIF MIT License
#
#  Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
#
#  Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
#  it is free of charge, to any person obtaining a copy of this software and associated
#  documentation files (the "Software"), to deal in the Software without restriction, including
#  without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the Software is furnished
#  to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all copies or
#  substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
#  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
#  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.



import sys
import os
import time
import argparse


LICENSE_STR = '''
/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

'''

GEN_HEAD_FILE_NAME =  "tone/audio_embed_tone.h"
GEN_CMAKE_FILE_NAME = "embed_tone.cmake"
GEN_MAKE_FILE_NAME = "compoenet.mk"

TONE_URL_DESCRIPTION = '\n/**\n * @brief embed tone url\n */'
TONE_URL_INDEX_DESCRIPTION = '\n/**\n * @brief embed tone url index for `embed_tone_url` array\n */'
TONE_CONTEXT_DESCRIPTION = '\n/**\n * @brief embed tone corresponding resource information, as a variable of the `embed_flash_stream_set_context` function\n */'

def gen_h_file(file_dic):
    """
    generate the c header file for audio embed tone
    """
    h_file = ''
    h_file += '#ifndef AUDIO_EMBED_TONE_URI_H\r\n#define AUDIO_EMBED_TONE_URI_H\r\n\r\n'
    h_file += "typedef struct {\r\n    const uint8_t * address;\r\n    int size;\r\n} embed_tone_t;\r\n\r\n"
    cmake_file = ''
    cmake_file = "set(COMPONENT_EMBED_TXTFILES"

    enum_file = ''
    struct_file = ''
    enum_file = TONE_URL_INDEX_DESCRIPTION
    enum_file += "\nenum tone_url_e {";
    struct_file = TONE_URL_DESCRIPTION
    struct_file += "\nconst char * embed_tone_url[] = {"
    next_h_file = TONE_CONTEXT_DESCRIPTION
    next_h_file += "\nembed_tone_t g_embed_tone[] = {"
    file_num = 0
    new_dict_file = dict()

    toupper_str = ''

    for key in file_dic:
        newkey = key.replace(".", "_")
        toupper_str = newkey.upper()
        print(toupper_str)
        h_file += "extern const uint8_t " + newkey + "[] asm(\"_binary_" + newkey + "_start\");\r\n"
        str_file_num = str(file_num)
        str_file_size = str(file_dic[key])
        next_h_file += "\n    [" + str_file_num + "] = {\n        .address = " + newkey + ",\n        .size    = "+ str_file_size +",\n        },"
        file_num += 1

        cmake_file += " " + key
        enum_file += "\n    " +  newkey.upper() + " = " + str_file_num + ","  #
        result = newkey.rfind("_")
        list_key=list(newkey)
        list_key[result] = '.'
        newkey = "".join(list_key)
        struct_file += "\n    " + "\"" + "embed://tone/" +str_file_num + "_" + newkey + "\"" +","
        print("newkey = %s" % newkey)

    enum_file += "\n    " +  "EMBED_TONE_URL_MAX" + " = " + str(file_num) + ","
    next_h_file += "\n};\n";
    cmake_file += ")"
    enum_file += "\n};\n\n";
    struct_file += "\n};\n\n";
    h_file += next_h_file;
    h_file += enum_file
    h_file += struct_file
    h_file += "#endif // AUDIO_EMBED_TONE_URI_H"
    return h_file, cmake_file

def write_h_file(file, file_name, path):
    if os.path.exists(file_name):
        os.remove(file_name)

    FileName = path + '/' + file_name
    with open(FileName,'w+') as f:
        if f != None:
            if GEN_HEAD_FILE_NAME == file_name:
                f.write(LICENSE_STR)
            f.write(file)
            f.close()

def write_cmake_file(file, path):
    ComponentName = path + '/' + GEN_CMAKE_FILE_NAME
    with open(ComponentName,'w+') as f:
        if f != None:
            f.truncate(0)
            f.write(file)
            f.close()

if __name__ == '__main__':

        argparser = argparse.ArgumentParser()
        argparser.add_argument('-p', '--path', type=str, required=True ,help='base folder for the source files generated')
        args = argparser.parse_args()

        file_list = [x for x in os.listdir(args.path) if ((x.endswith(".wav")) or (x.endswith(".mp3")))]
        file_list.sort()
        print('-------------------')
        print(file_list)
        print('-------------------')


        if os.path.exists("tone") == False:
            os.mkdir("tone")

        dict_file = dict()
        for i in file_list:
            size = os.path.getsize(i)
            dict_file[i] = size
        for j in dict_file:
            print("name: %s" % j)
            print("size: %d" % dict_file[j])
            print('\n')
        h_context, cmake_context  = gen_h_file(dict_file)
        write_h_file(h_context, GEN_HEAD_FILE_NAME, args.path)
        write_cmake_file(cmake_context, args.path)
