#! /bin/bash

MATRIX=(
    "mid"
)

TH=(
    4
    8
    12
    16
    32
)

PF_TYPE=(
    "stride"
    "isbp"
)

ST_DG=(
    2
    4
    6
    8
)

STATS=(
    "simSeconds"
    "system.mem_ctrls.bwTotal::total"
)

CHOOSE_STATS=${STATS[1]}
#for CHOOSE_STATS in "${STATS[@]}"; do

:> "$CHOOSE_STATS.txt"
for pf in "${PF_TYPE[@]}"; do
    RES=""

    # make header
    HEADER=$pf" "
    for dg in "${ST_DG[@]}"; do
        HEADER=$HEADER"DG$dg "
    done
    RES+=$HEADER"NOPF \n"

    # gather stats value
    for th in "${TH[@]}"; do
        LINE="TH$th "
        for dg in "${ST_DG[@]}"; do
            STAT_FILE="${MATRIX[0]}_TH${th}/${pf}_TH${th}_DG${dg}_m5out/stats.txt"
            VAL=`awk -v pattern=$CHOOSE_STATS '$0 ~ pattern {print $2; exit}' $STAT_FILE`
            LINE=$LINE"$(echo "scale=2; $VAL / 1024 / 1024 / 1024" | bc) "
        done
        NOPF_STAT_FILE="${MATRIX[0]}_TH${th}/nopf_m5out/stats.txt"
        VAL=`awk -v pattern=$CHOOSE_STATS '$0 ~ pattern {print $2; exit}' $NOPF_STAT_FILE`
        LINE=$LINE"$(echo "scale=2; $VAL / 1024 / 1024 / 1024" | bc) "

        RES+=$LINE"\n"
    done
    echo -e $RES | column -t >> "$CHOOSE_STATS.txt"
    echo "" >> "$CHOOSE_STATS.txt"
done

#done

