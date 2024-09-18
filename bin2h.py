#!/usr/bin/python3
import argparse

def main():
  parser = argparse.ArgumentParser(prog="bin2h", description="convert binary file to header", epilog="")
  parser.add_argument("-b", "--bin", help="path to binary file", required=True)
  parser.add_argument("-c", "--code", help="path to output header file", required=True)
  parser.add_argument("-v", "--variable", help="name of generated variable in header", required=True)

  args = parser.parse_args()
  with open(args.bin, mode="rb") as f:
    data = list(f.read())
  columns = 12
  with open(args.code, mode="w") as f:
    f.write("const unsigned char {}[] = {{".format(args.variable))
    for i in range(len(data)):
      if i % columns == 0:
        f.write("\n    ")
      else:
        f.write(" ")
      elem = data[i]
      f.write("0x{}".format(format(elem, "02x")))
      if i != len(data) - 1:
        f.write(",")
    f.write("}};\nconst int {}_size = {};\n".format(args.variable, len(data)))

if __name__ == "__main__":
  main()

