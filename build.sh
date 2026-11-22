set -xe

CMD="gcc -o atrds.out -std=c99"
# CMD="$CMD -Werror -Wall -Wformat -Wextra -Wvla"
CMD="$CMD -fsanitize=address,undefined -O0 -g"
CMD="$CMD ./src/*.c" # compile tous les fichiers .c du dossier src
CMD="$CMD -lm"
eval $CMD
