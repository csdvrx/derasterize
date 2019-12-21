#!/bin/sh
# Run the aart output though that script to check the frequency of the unicode used 
echo "Unicode tally of uaart output:"
echo
cat $1 | sed 's/\(.\)/\1\n/g' |sort|uniq -ic| \
# sort by count and remove the non unicode
sort -nr -k1 | grep -v '[0-9m;\[]$' | tr -d '\n'

echo
