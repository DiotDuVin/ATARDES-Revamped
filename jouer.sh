#!/usr/bin/env bash
#
# Lance ATARDES dans une nouvelle fenêtre de terminal, en plein écran,
# avec une taille de police calculée pour que le jeu tienne à l'écran.
#
# Utilisation :
#   ./jouer.sh                 lance le jeu
#   ./jouer.sh --play          les options sont transmises au jeu (voir ./atrds.out --help)
#   ./jouer.sh --dry-run       affiche la commande sans rien lancer
#
# Variables optionnelles :
#   ATARDES_TERMINAL=kitty     force un terminal (kitty, alacritty, wezterm, konsole,
#                              gnome-terminal, mate-terminal, xfce4-terminal, xterm)
#   ATARDES_FONT_SIZE=7.5      force la taille de police (en points)
#   ATARDES_SCREEN=1920x1080   force la résolution de l'écran

cd "$(dirname "$(readlink -f "$0")")" || exit 1

# Taille minimale demandée par le jeu (voir MIN_TERMINAL_WIDTH / MIN_TERMINAL_HEIGHT dans src/shared.h)
COLS=232
ROWS=74
MAX_COLS=350 # MAX_TERMINAL_WIDTH / MAX_TERMINAL_HEIGHT
MAX_ROWS=100

DRY_RUN=0
GAME_ARGS=()
for arg in "$@"; do
    if [ "$arg" = "--dry-run" ]; then DRY_RUN=1; else GAME_ARGS+=("$arg"); fi
done

# 1. Compilation si le jeu n'existe pas ou si le code a changé
if [ ! -x ./atrds.out ] || [ -n "$(find src -name '*.[ch]' -newer atrds.out 2>/dev/null | head -n 1)" ]; then
    echo "Compilation du jeu..."
    ./build.sh > /dev/null 2>&1 || { echo "La compilation a échoué : lance ./build.sh pour voir l'erreur."; exit 1; }
fi

# 2. Si le terminal actuel a déjà la bonne taille, on lance directement le jeu ici
if [ "$DRY_RUN" = 0 ] && [ -t 0 ] && [ -t 1 ]; then
    read -r cur_rows cur_cols < <(stty size 2>/dev/null)
    if [ "${cur_cols:-0}" -ge "$COLS" ] && [ "${cur_rows:-0}" -ge "$ROWS" ] \
        && [ "${cur_cols:-0}" -le "$MAX_COLS" ] && [ "${cur_rows:-0}" -le "$MAX_ROWS" ]; then
        exec ./atrds.out "${GAME_ARGS[@]}"
    fi
fi

# 3. Résolution de l'écran (en pixels)
screen_size() {
    if [ -n "$ATARDES_SCREEN" ]; then echo "${ATARDES_SCREEN/x/ }"; return; fi
    local size=""
    if command -v xrandr > /dev/null 2>&1; then
        size=$(xrandr --current 2>/dev/null | grep -oE '[0-9]+x[0-9]+\+0\+0' | head -n 1 | cut -d+ -f1)
        [ -z "$size" ] && size=$(xrandr --current 2>/dev/null | grep '\*' | head -n 1 | grep -oE '[0-9]+x[0-9]+' | head -n 1)
    fi
    if [ -z "$size" ] && command -v xdpyinfo > /dev/null 2>&1; then
        size=$(xdpyinfo 2>/dev/null | awk '/dimensions:/ {print $2; exit}')
    fi
    [ -z "$size" ] && size="1920x1080" # valeur par défaut si on ne trouve pas
    echo "${size/x/ }"
}
read -r SCREEN_W SCREEN_H < <(screen_size)

# Prise en compte du facteur d'échelle de l'écran (écrans haute résolution sous GNOME/MATE)
SCALE=1
if command -v gsettings > /dev/null 2>&1; then
    s=$(gsettings get org.gnome.desktop.interface scaling-factor 2>/dev/null | grep -oE '[0-9]+$')
    [ -n "$s" ] && [ "$s" -gt 1 ] && SCALE=$s
    s=$(gsettings get org.mate.interface window-scaling-factor 2>/dev/null | grep -oE '[0-9]+$')
    [ -n "$s" ] && [ "$s" -gt 1 ] && SCALE=$s
fi

# 4. Taille de police (en points) pour que COLS x ROWS caractères tiennent dans l'écran.
#    Approximations pour une police monospace : largeur ≈ 0.6 x taille, hauteur de ligne ≈ 1.25 x taille.
#    1 point = 4/3 pixels à 96 DPI. On garde une petite marge (barre de titre, bords).
if [ -n "$ATARDES_FONT_SIZE" ]; then
    FONT_PT=$ATARDES_FONT_SIZE
else
    FONT_PT=$(awk -v w="$SCREEN_W" -v h="$SCREEN_H" -v s="$SCALE" -v c="$COLS" -v r="$ROWS" 'BEGIN {
        w /= s; h /= s;
        px_w = (w * 0.97) / (c + 2) / 0.6;
        px_h = (h * 0.95) / (r + 1) / 1.25;
        px = (px_w < px_h) ? px_w : px_h;
        pt = int(px * 0.75 * 2) / 2;   # arrondi au demi-point inférieur
        if (pt < 4) pt = 4;
        printf "%.1f", pt }')
fi

# Police monospace par défaut du bureau (pour calculer le zoom des terminaux qui n'acceptent qu'un zoom)
default_font_pt() {
    local f=""
    if command -v gsettings > /dev/null 2>&1; then
        f=$(gsettings get org.gnome.desktop.interface monospace-font-name 2>/dev/null)
        [ -z "$f" ] && f=$(gsettings get org.mate.interface monospace-font-name 2>/dev/null)
    fi
    f=$(echo "$f" | grep -oE '[0-9]+(\.[0-9]+)?' | tail -n 1)
    echo "${f:-11}"
}

# 5. Choix du terminal : celui depuis lequel on lance le script si possible, sinon le premier installé
KNOWN="kitty alacritty wezterm-gui wezterm konsole gnome-terminal mate-terminal xfce4-terminal xterm"
detect_parent_terminal() {
    local pid=$$ name
    for _ in $(seq 1 15); do
        pid=$(ps -o ppid= -p "$pid" 2>/dev/null | tr -d ' ')
        [ -z "$pid" ] || [ "$pid" -le 1 ] && return
        name=$(ps -o comm= -p "$pid" 2>/dev/null)
        case "$name" in
            kitty|alacritty|wezterm*|konsole|xterm|xfce4-terminal|mate-terminal*) echo "${name%%-gui}"; return ;;
            gnome-terminal*) echo gnome-terminal; return ;;
        esac
    done
}
TERMINAL=${ATARDES_TERMINAL:-$(detect_parent_terminal)}
if [ -z "$TERMINAL" ] || ! command -v "$TERMINAL" > /dev/null 2>&1; then
    TERMINAL=""
    for t in $KNOWN; do
        if command -v "$t" > /dev/null 2>&1; then TERMINAL=${t%%-gui}; break; fi
    done
fi
if [ -z "$TERMINAL" ] || { [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; }; then
    echo "Impossible d'ouvrir une nouvelle fenêtre : lance le jeu dans un terminal en plein écran et dézoome avec Ctrl -."
    [ "$DRY_RUN" = 1 ] && exit 1
    ATARDES_LAUNCHED=1 exec ./atrds.out "${GAME_ARGS[@]}" # le jeu affichera les instructions
fi

# Script exécuté dans la nouvelle fenêtre. Si le jeu trouve que la taille n'est toujours pas bonne,
# il quitte avec le code 42 et écrit dans RATIO_FILE de combien corriger la police :
# on rouvre alors une fenêtre avec la police corrigée (2 essais maximum).
RETRY=${ATARDES_RETRY:-0}
# Options du jeu, protégées pour être recopiées dans le script (vide s'il n'y en a pas :
# attention, printf '%q ' sur une liste vide produirait un argument vide '')
QUOTED_ARGS=""
[ ${#GAME_ARGS[@]} -gt 0 ] && QUOTED_ARGS=$(printf '%q ' "${GAME_ARGS[@]}")
RUN_SCRIPT=$(mktemp "${TMPDIR:-/tmp}/atardes-XXXXXX.sh")
RATIO_FILE="$RUN_SCRIPT.ratio"
{
    echo "cd $(printf '%q' "$PWD") || exit 1"
    echo "export ATARDES_LAUNCHED=1 ATARDES_RETRY=$RETRY ATARDES_RATIO_FILE=$(printf '%q' "$RATIO_FILE")"
    echo "./atrds.out $QUOTED_ARGS"
    echo 'code=$?'
    echo "if [ \$code -eq 42 ] && [ -s $(printf '%q' "$RATIO_FILE") ]; then"
    echo "    font=\$(awk -v p=$FONT_PT -v r=\"\$(cat $(printf '%q' "$RATIO_FILE"))\" 'BEGIN { v = int(p * r * 2) / 2; if (v < 3) v = 3; printf \"%.1f\", v }')"
    echo "    rm -f $(printf '%q' "$RATIO_FILE") \"\$0\""
    echo "    unset ATARDES_LAUNCHED"
    echo "    ATARDES_FONT_SIZE=\$font ATARDES_RETRY=$((RETRY + 1)) ATARDES_TERMINAL=$TERMINAL exec ./jouer.sh $QUOTED_ARGS"
    echo "fi"
    echo "rm -f $(printf '%q' "$RATIO_FILE") \"\$0\""
    echo "echo; read -rsn1 -p 'Appuie sur une touche pour fermer...'"
} > "$RUN_SCRIPT"
RUN="bash $(printf '%q' "$RUN_SCRIPT")"
ZOOM=$(awk -v p="$FONT_PT" -v d="$(default_font_pt)" 'BEGIN { printf "%.2f", p / d }')

case "$TERMINAL" in
    kitty)          CMD=(kitty -o "font_size=$FONT_PT" --start-as=fullscreen bash -c "$RUN") ;;
    alacritty)      CMD=(alacritty -o "font.size=$FONT_PT" -o 'window.startup_mode="Fullscreen"' -e bash -c "$RUN") ;;
    wezterm)        CMD=(wezterm --config "font_size=$FONT_PT" --config "initial_cols=$COLS" --config "initial_rows=$ROWS" start --always-new-process -- bash -c "$RUN") ;;
    konsole)        CMD=(konsole --fullscreen -p "Font=Monospace,$FONT_PT" -e bash -c "$RUN") ;;
    gnome-terminal) CMD=(gnome-terminal --full-screen "--zoom=$ZOOM" -- bash -c "$RUN") ;;
    mate-terminal)  CMD=(mate-terminal --full-screen "--zoom=$ZOOM" -x bash -c "$RUN") ;;
    xfce4-terminal) CMD=(xfce4-terminal --fullscreen "--font=Monospace $FONT_PT" -x bash -c "$RUN") ;;
    xterm)          CMD=(xterm -fa Monospace -fs "$FONT_PT" -fullscreen -e bash -c "$RUN") ;;
    *)              echo "Terminal non géré : $TERMINAL"; exit 1 ;;
esac

echo "Écran ${SCREEN_W}x${SCREEN_H} (échelle x$SCALE) -> police $FONT_PT pt dans $TERMINAL"
if [ "$DRY_RUN" = 1 ]; then
    printf '%q ' "${CMD[@]}"
    echo
    echo "--- script lancé dans la fenêtre :"
    cat "$RUN_SCRIPT"
    rm -f "$RUN_SCRIPT"
    exit 0
fi
echo "Si le jeu demande encore de dézoomer : Ctrl - (ou ATARDES_FONT_SIZE=6 ./jouer.sh)"
# setsid détache complètement la nouvelle fenêtre : elle ne se ferme pas quand ce terminal se ferme
if command -v setsid > /dev/null 2>&1; then
    setsid "${CMD[@]}" > /dev/null 2>&1 < /dev/null &
else
    nohup "${CMD[@]}" > /dev/null 2>&1 < /dev/null &
fi
disown 2> /dev/null
sleep 0.5 # laisse le temps à la fenêtre de démarrer avant que ce terminal ne se ferme éventuellement
