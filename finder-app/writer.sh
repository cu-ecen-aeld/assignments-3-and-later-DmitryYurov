#!/bin/sh

if [ $# -ne 2 ]
then
    echo "Not all the necessary arguments were specified"
    exit 1
fi

mkdir -p "$(dirname "$1")" && touch "$1"
if [ $? -ne 0 ]
then
    echo "Couldn't create a path or the file"
    exit 1
fi

echo "$2" >> "$1"