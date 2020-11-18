#! /usr/bin/env python3
#coding=utf-8

def main():
    import os
    import sys
    import xml.etree.ElementTree as ET
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('xml', help = 'xml path')

    args = parser.parse_args()

    fp = args.xml
    if not os.path.exists(fp):
        print("Error: file '%s' not exist"%(fp), file=sys.stderr)
    tree = ET.parse(fp)
    root = tree.getroot()
    for i in root.findall("./builds_flat/build"):
        d = {}
        for j in i:
            d[j.tag] = j.text
        if d.get("name") == "wlan":
            print(d["linux_root_path"])

if __name__=="__main__":
    main()
