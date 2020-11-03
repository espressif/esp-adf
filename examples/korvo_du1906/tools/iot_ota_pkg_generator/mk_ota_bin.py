"""
ota bin generate script
"""
import os
import argparse

# python3 mk_ota_bin.py -c tda -st 1.0.1 -sd 1.0.1 -sa 1.0.1 -v 103 -fd DU1906_slave_v1.4.8.E.bin
# python3 mk_ota_bin.py -c t -st 1.0.1 -v 103 -fd DU1906_slave_v1.4.8.E.bin
# python3 mk_ota_bin.py -c d -sd 1.0.1 -v 103 -fd DU1906_slave_v1.4.8.E.bin
# python3 mk_ota_bin.py -c a -sa 1.0.1 -v 103 -fd DU1906_slave_v1.4.8.E.bin
# python3 mk_ota_bin.py -c td -st 1.0.1 -sd 1.0.1 -v 103 -fd DU1906_slave_v1.4.8.E.bin
# python3 mk_ota_bin.py -c ta -st 1.0.1 -sa 1.0.1 -v 103 -fd DU1906_slave_v1.4.8.E.bin
# python3 mk_ota_bin.py -c da -sd 1.0.1 -sa 1.0.1 -v 103 -fd DU1906_slave_v1.4.8.E.bin
parser = argparse.ArgumentParser()
parser.add_argument("-c", "--config", required = True, help = "Configuration type to OTA. combine, app, tone or dsp")
parser.add_argument("-st", "--sub_tone", help = "Sub version of tone bin. example: 1.0.0")
parser.add_argument("-sd", "--sub_dsp", help = "Sub version of dsp bin. example: 1.0.0")
parser.add_argument("-sa", "--sub_app", help = "Sub version of app bin. example: 1.0.0")
parser.add_argument("-v", "--version", required = True, help = "OTA package version. example: 105")
parser.add_argument("-p", "--path", default = ".", help = "Project abs or relative path.")
parser.add_argument("-fd", "--file_dsp", default = "DU1906_slave_v1.5.5.D.bin", help = "Dsp firmware file.")
args = parser.parse_args()
#print(args.input)

ota_bin_file = 'combine_ota_default.bin'
if args.config == 'tda':
    tone_file = args.path + '/tone/audio_tone.bin'
    dsp_file = args.path + '/firmware/' + args.file_dsp
    app_file = args.path + '/build/korvo_du1906.bin'
    tone_size = os.path.getsize(tone_file)
    dsp_size = os.path.getsize(dsp_file)
    app_size = os.path.getsize(app_file)
    offset1 = 1024 + tone_size
    offset2 = offset1 + dsp_size
    with open(ota_bin_file, 'w') as f:
        dummy_bytes = "\0" * 1024
        f.write(dummy_bytes)
        f.seek(0)
        f.write('{\n')
        f.write('    "version": ' + args.version + ',\n')
        f.write('    "tone": {\n')
        f.write('        "sub_ver": "' + args.sub_tone + '",\n')
        f.write('        "desc": "app tone file",\n')
        f.write('        "offset": 1024,\n')
        f.write('        "length": ' + str(tone_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    },\n')
        f.write('    "dsp": {\n')
        f.write('        "sub_ver": "' + args.sub_dsp + '",\n')
        f.write('        "desc": "dsp file",\n')
        f.write('        "offset": ' + str(offset1) + ',\n')
        f.write('        "length": ' + str(dsp_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    },\n')
        f.write('    "app": {\n')
        f.write('        "sub_ver": "' + args.sub_app + '",\n')
        f.write('        "desc": "app file",\n')
        f.write('        "offset": ' + str(offset2) + ',\n')
        f.write('        "length": ' + str(app_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    }\n')
        f.write('}\n')
        f.write('\0')
        f.close()

    out_data = b''
    with open(tone_file, 'rb') as tf:
        out_data += tf.read()
    with open(dsp_file, 'rb') as df:
        out_data += df.read()
    with open(app_file, 'rb') as af:
        out_data += af.read()
    with open(ota_bin_file, 'ab') as of:
        of.seek(1024)
        of.write(out_data)

elif args.config == 't':
    tone_file = args.path + '/tone/audio_tone.bin'
    tone_size = os.path.getsize(tone_file)
    with open(ota_bin_file, 'w') as f:
        dummy_bytes = "\0" * 1024
        f.write(dummy_bytes)
        f.seek(0)
        f.write('{\n')
        f.write('    "version": ' + args.version + ',\n')
        f.write('    "tone": {\n')
        f.write('        "sub_ver": "' + args.sub_tone + '",\n')
        f.write('        "desc": "app tone file",\n')
        f.write('        "offset": 1024,\n')
        f.write('        "length": ' + str(tone_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    }\n')
        f.write('}\n')
        f.write('\0')
        f.close()

    out_data = b''
    with open(tone_file, 'rb') as tf:
        out_data += tf.read()
    
    with open(ota_bin_file, 'ab') as of:
        of.seek(1024)
        of.write(out_data)
elif args.config == 'd':
    dsp_file = args.path + '/firmware/' + args.file_dsp
    dsp_size = os.path.getsize(dsp_file)
    with open(ota_bin_file, 'w') as f:
        dummy_bytes = "\0" * 1024
        f.write(dummy_bytes)
        f.seek(0)
        f.write('{\n')
        f.write('    "version": ' + args.version + ',\n')
        f.write('    "dsp": {\n')
        f.write('        "sub_ver": "' + args.sub_dsp + '",\n')
        f.write('        "desc": "dsp file",\n')
        f.write('        "offset": 1024,\n')
        f.write('        "length": ' + str(dsp_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    }\n')
        f.write('}\n')
        f.write('\0')
        f.close()

    out_data = b''
    with open(dsp_file, 'rb') as df:
        out_data += df.read()
    with open(ota_bin_file, 'ab') as of:
        of.seek(1024)
        of.write(out_data)

elif args.config == 'a':
    app_file = args.path + '/build/korvo_du1906.bin'
    app_size = os.path.getsize(app_file)
    with open(ota_bin_file, 'w') as f:
        dummy_bytes = "\0" * 1024
        f.write(dummy_bytes)
        f.seek(0)
        f.write('{\n')
        f.write('    "version": ' + args.version + ',\n')
        f.write('    "app": {\n')
        f.write('        "sub_ver": "' + args.sub_app + '",\n')
        f.write('        "desc": "app file",\n')
        f.write('        "offset": 1024,\n')
        f.write('        "length": ' + str(app_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    }\n')
        f.write('}\n')
        f.write('\0')
        f.close()

    out_data = b''
    with open(app_file, 'rb') as af:
        out_data += af.read()
    with open(ota_bin_file, 'ab') as of:
        of.seek(1024)
        of.write(out_data)

elif args.config == 'td':
    tone_file = args.path + '/tone/audio_tone.bin'
    dsp_file = args.path + '/firmware/' + args.file_dsp
    tone_size = os.path.getsize(tone_file)
    dsp_size = os.path.getsize(dsp_file)
    offset1 = 1024 + tone_size
    with open(ota_bin_file, 'w') as f:
        dummy_bytes = "\0" * 1024
        f.write(dummy_bytes)
        f.seek(0)
        f.write('{\n')
        f.write('    "version": ' + args.version + ',\n')
        f.write('    "tone": {\n')
        f.write('        "sub_ver": "' + args.sub_tone + '",\n')
        f.write('        "desc": "app tone file",\n')
        f.write('        "offset": 1024,\n')
        f.write('        "length": ' + str(tone_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    },\n')
        f.write('    "dsp": {\n')
        f.write('        "sub_ver": "' + args.sub_dsp + '",\n')
        f.write('        "desc": "dsp file",\n')
        f.write('        "offset": ' + str(offset1) + ',\n')
        f.write('        "length": ' + str(dsp_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    }\n')
        f.write('}\n')
        f.write('\0')
        f.close()

    out_data = b''
    with open(tone_file, 'rb') as tf:
        out_data += tf.read()
    with open(dsp_file, 'rb') as df:
        out_data += df.read()
    with open(ota_bin_file, 'ab') as of:
        of.seek(1024)
        of.write(out_data)

elif args.config == 'ta':
    tone_file = args.path + '/tone/audio_tone.bin'
    app_file = args.path + '/build/korvo_du1906.bin'
    tone_size = os.path.getsize(tone_file)
    app_size = os.path.getsize(app_file)
    offset1 = 1024 + tone_size
    with open(ota_bin_file, 'w') as f:
        dummy_bytes = "\0" * 1024
        f.write(dummy_bytes)
        f.seek(0)
        f.write('{\n')
        f.write('    "version": ' + args.version + ',\n')
        f.write('    "tone": {\n')
        f.write('        "sub_ver": "' + args.sub_tone + '",\n')
        f.write('        "desc": "app tone file",\n')
        f.write('        "offset": 1024,\n')
        f.write('        "length": ' + str(tone_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    },\n')
        f.write('    "app": {\n')
        f.write('        "sub_ver": "' + args.sub_app + '",\n')
        f.write('        "desc": "app file",\n')
        f.write('        "offset": ' + str(offset1) + ',\n')
        f.write('        "length": ' + str(app_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    }\n')
        f.write('}\n')
        f.write('\0')
        f.close()

    out_data = b''
    with open(tone_file, 'rb') as tf:
        out_data += tf.read()
    with open(app_file, 'rb') as af:
        out_data += af.read()
    with open(ota_bin_file, 'ab') as of:
        of.seek(1024)
        of.write(out_data)

elif args.config == 'da':
    dsp_file = args.path + '/firmware/' + args.file_dsp
    app_file = args.path + '/build/korvo_du1906.bin'
    dsp_size = os.path.getsize(dsp_file)
    app_size = os.path.getsize(app_file)
    offset1 = 1024 + dsp_size
    with open(ota_bin_file, 'w') as f:
        dummy_bytes = "\0" * 1024
        f.write(dummy_bytes)
        f.seek(0)
        f.write('{\n')
        f.write('    "version": ' + args.version + ',\n')
        f.write('    "dsp": {\n')
        f.write('        "sub_ver": "' + args.sub_dsp + '",\n')
        f.write('        "desc": "dsp file",\n')
        f.write('        "offset": 1024,\n')
        f.write('        "length": ' + str(dsp_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    },\n')
        f.write('    "app": {\n')
        f.write('        "sub_ver": "' + args.sub_app + '",\n')
        f.write('        "desc": "app file",\n')
        f.write('        "offset": ' + str(offset1) + ',\n')
        f.write('        "length": ' + str(app_size) + ',\n')
        f.write('        "checksum": "abcdef"\n')
        f.write('    }\n')
        f.write('}\n')
        f.write('\0')
        f.close()

    out_data = b''
    with open(dsp_file, 'rb') as df:
        out_data += df.read()
    with open(app_file, 'rb') as af:
        out_data += af.read()
    with open(ota_bin_file, 'ab') as of:
        of.seek(1024)
        of.write(out_data)

