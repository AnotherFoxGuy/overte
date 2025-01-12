import os
import re

# Regular expressions to find the Qt specific strings
patterns = [r'Q_OBJECT', r'Q_GADGET', r'Q_NAMESPACE', r'Q_NAMESPACE_EXPORT', 
            r'Q_GADGET_EXPORT', r'Q_ENUM_NS']

def search_file(filename):
    with open(filename, 'r') as file:
        for line in file:
            # Check if any pattern is found in the line
            if any(re.search(pattern, line) for pattern in patterns):
                print(filename)
                return True
    return False

def traverse_directory(directory):
    for root, dirs, files in os.walk(directory):
        for file in files:
            search_file(os.path.join(root, file))

# Call the function with your directory path
traverse_directory('src')