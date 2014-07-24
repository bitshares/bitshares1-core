#!/usr/bin/python

def isfloat(value):
  try:
    float(value)
    return True
  except ValueError:
    return False

import sys

if len(sys.argv) == 3:
  infile = sys.argv[1]
  outfile = sys.argv[2]
else:
  infile = raw_input("Enter input file:")
  outfile = raw_input("Enter output file:")

with open(infile, "r") as gen:
  with open(outfile, "w") as out:
    for line in gen:
        if isfloat(line):
            out.write(" " * 12 + str(int(float(line) * 1000000)) + "\n")
        else:
            out.write(line)