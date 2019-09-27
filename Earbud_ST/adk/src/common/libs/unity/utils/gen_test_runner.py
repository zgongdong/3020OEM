#
#    Copyright (c) 2014 - 2015 Qualcomm Technologies International, Ltd.
#

import sys
import os
import re

if len(sys.argv) != 2:
	print "Provide file name to parse"
	exit()

input_filename = sys.argv[1]

filename_with_path = os.path.realpath(input_filename)
filename = os.path.basename(filename_with_path)
directory = os.path.dirname(filename_with_path)

fn_list = list()

fh = open(filename_with_path, 'r')
for line in fh:
	re_match = re.match("^(static)?\\s*void\\s*(test\\w+)", line)
	if re_match:
		fn_list.append(re_match.group(2))
fh.close()

template_top = [
	'#include \"unity.h\"\n',
	'\n',
	'extern void setUp(void);\n',
	'extern void tearDown(void);\n\n'
]

template_mid = [
	'\nint main()\n',
	'{\n',
]

template_bottom = [
	'\n	UNITY_END();\n\n',
	'	return Unity.TestFailures;\n',
	'}\n',
]

fh = open(os.path.join(directory, "test_runner.c"), 'w')

fh.writelines(template_top)

for func in fn_list:
	fh.write("extern void " + func + "(void);\n")

fh.writelines(template_mid)

fh.write('	UNITY_BEGIN("' + filename +' tests");\n\n')

for func in fn_list:
	fh.write("	RUN_TEST(" + func + ");\n")

fh.writelines(template_bottom)

fh.close
