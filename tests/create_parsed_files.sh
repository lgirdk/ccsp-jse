#!/bin/bash

# see README.txt for details but the summary is:
# 1) make desktop build incide jst/build folder
# 2) run this file from inside the jst/tests/parser directory

cp ../../jsts/php.jst ./include
cp ../../jsts/*.js ./

find ./ -name "*.jst" -maxdepth 1 -exec sh -c "../../build/jst --parse-only {} > {}.parsed" \;

echo "now run the following:"
echo "git add tests/parser/* tests/parser/include/*"

