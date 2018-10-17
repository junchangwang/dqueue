#!/usr/bin/env python3

import sys
import glob
import collections

files = []
content = {}

def load_logs():
    global files
    files = glob.glob(sys.argv[1] + "/*")
    files.sort()
    print('  To processce log files: [%s] \n' % ', '.join(map(str, files)))
    for i in range(0, len(files)):
        content[i] = set(map(int, open(files[i]).read().split()))

def has_duplicates(list_of_values):
    value_dict = collections.defaultdict(int)
    for item in list_of_values:
        value_dict[item] += 1
    for item in value_dict:
        if value_dict[item] > 1:
            print("  == Duplicate elements: [%s][%d times] ==" % (item, value_dict[item]))

def check_each_log_no_duplicate():
    print("  ====\n  To check no duplicate elements in each log file:")
    for i in range(0, len(files)):
        temp_list = list(map(int, open(files[i]).read().split()))
        if len(temp_list) != len(content[i]):
            print('  == ERROR: Duplicate elements in [%s]. List size: %d, Set size: %d ==' % (files[i], len(temp_list), len(content[i])))
            has_duplicates(temp_list)
            sys.exit(-1)
    print("  PASS  \n")

def check_global_no_duplicate():
    print("  ====\n  To check no duplicate elements enqueued by producers:")
    temp_list = []
    temp_set = set()
    for i in range(1, len(files)):
        temp_list = temp_list + list(open(files[i]).read().split())
        temp_set.update(content[i])

    if len(temp_list) != len(temp_set):
        print('  == ERROR: Data appear in producer-full for multiple times. List size: %d, Set size: %d ==' % (len(temp_list), len(temp_set)))
        for x in temp_set:
            temp_list.remove(x)
        print("  == Duplicate elements: [%s] ==" % ', '.join(map(str,temp_list)))
        sys.exit(-1)
    print("  PASS  \n")

def check_no_missing():
    print("  ====\n  To check no missing element in data dequeued: ")
    temp_list = list(content[0]) # We have checked there is no redundancy
    temp_list.sort()
    if ((max(temp_list) - min(temp_list) + 1) != len(temp_list)):
        print('  == ERROR: Missing elements in data dequeued ==')
        full_list = list(range(min(temp_list), max(temp_list) + 1))
        for x in temp_list:
            full_list.remove(x)
        print("  Missing elements: [%s]" % ', '.join(map(str,full_list)))
        sys.exit(-1)
    print("  PASS  \n")

def check_enqueued_dequeued_equal():
    print("  ====\n  To check element sets enqueued and dequeued are identical: ")
    temp_consumer = content[0]
    temp_producer = set()
    for i in range(1, len(files)):
        temp_producer.update(content[i])
    if (temp_consumer != temp_producer):
        print('  == ERROR: Elements sets enqueued and dequeued are not match ==')
        print("            Missing elements (producer - consumer): [%s]" % ', '.join(map(str,list(temp_consumer - temp_producer))))
        print("            Missing elements (consumer - producer): [%s]" % ', '.join(map(str,list(temp_producer - temp_consumer))))
        sys.exit(-1)
    print("  PASS  \n")

def check_sequential_consistency():
    print("  ====\n  To check if the program is sequential consistent: ")
    list_consumer = list(map(int, open(files[0]).read().split()))
    list_producers = {}
    for i in range(1, len(files)):
        list_producers[i] = list(map(int, open(files[i]).read().split()))
    for x in list_consumer:
        found = 0
        for j in range(1, len(files)):
            if list_producers[j] and (x == list_producers[j][0]):
                list_producers[j].pop(0)
                found = 1
                break;
        if (found == 0):
            print('  == ERROR: sequential consistencyi breaks. Element: [%d]  ==' % x)
            sys.exit(-1)


if len(sys.argv) < 2 :
    print("Usage: ./verify.py <log directory>")
    exit(0)

print("  Start verifying log files in " + sys.argv[1])
load_logs()
check_each_log_no_duplicate()
check_global_no_duplicate()
check_no_missing()
check_enqueued_dequeued_equal()
#check_sequential_consistency()
