#!/bin/sh

if [ $# -ne 2 ]
then
    echo "Not all the necessary arguments were specified"
    exit 1
elif [ ! -d $1 ]
then
    echo "$1 is not a directory"
    exit 1
fi

n_files=$(find $1 -type f | wc -l)
n_lines=$(grep -r $2 $1 | wc -l)

echo "The number of files are $n_files and the number of matching lines are $n_lines"