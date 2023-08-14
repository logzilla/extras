# LogZilla NEO Files

## rules.d

This directory contains rules for adding the meta tags. We've included a script to generate rules based on a tab separated input file, located in the [tsv2NEO directory](tsv2NEO)

## scripts

This directory contains scripts fired by NEO triggers.

You will need to edit the [`getAP`](scripts) perl script, then copy it to the container:

```
cd scripts
cp ./getAP /var/lib/docker/volumes/lz_data/_data/scripts/
```

