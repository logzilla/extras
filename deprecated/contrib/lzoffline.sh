#!/bin/bash
# To use this script:
# wget 'https://raw.githubusercontent.com/logzilla/extras/master/contrib/lzoffline.sh'
# chmod 755 ./lzoffline.sh
# ./lzoffline.sh -h

RED="\e[31m"
GREEN="\e[32m"
BLUE="\e[44m"
NC='\033[0m' # No Color
sourceDir="/var/lib/docker/volumes/lz_archive/_data"
modTime=5

ask() {
  local prompt default reply
  while true; do
    if [ "${2:-}" = "Y" ]; then
      prompt="Y/n"
      default=Y
    elif [ "${2:-}" = "N" ]; then
      prompt="y/N"
      default=N
    else
      prompt="y/n"
      default=
    fi
    echo -n "$1 [$prompt] "
    read reply </dev/tty
    if [ -z "$reply" ]; then
      reply=$default
    fi
    case "$reply" in
      Y*|y*) return 0 ;;
      N*|n*) return 1 ;;
    esac
  done
}

usage() {
  echo
  echo -en "This script is used to move and restore LogZilla archives to/from cold storage\n"
  echo -en "   h         This help\n"
  echo -en "   a         Archive data to cold storage\n"
  echo -en "   r         Restore an old archive\n"
  echo -en "   m         Set mod time, ${BLUE}default:${NC} 5 days\n"
  echo -en "   s         Source archive directory, ${BLUE}default:${NC} $sourceDir\n"
  echo -en "   d         Destination archive directory, ${BLUE}required if using -a${NC}\n"
  echo -en "   t         Tar file path, ${BLUE}required if using -t${NC}\n"
  exit 1
}

[[ -z $1 ]] && usage


while getopts 'harm:s:d:t:' opt ; do
  case "$opt" in
    h) usage
      ;;
    m) modTime="$OPTARG"
      ;;
    s) sourceDir="$OPTARG"
      ;;
    d) destDir="$OPTARG"
      ;;
    a) archive=1
      ;;
    t) tarFile="$OPTARG"
      ;;
    r) restore=1
      ;;
    \? )
      echo "Invalid option: $OPTARG" 1>&2; exit 1
      ;;
    : )
      echo "Invalid option: $OPTARG requires an argument" 1>&2
      ;;
  esac
done

archive () {
  [[ -z "$destDir" ]] && { echo -e "${RED}ERROR:${NC} Missing '-d' (destination directory) for archives, e.g.:\n     $0 -a -d /mnt/foo\n"; exit 1; }
  echo -e "\n${GREEN}ARCHIVING:${NC} Last $modTime days of data from ${sourceDir} to ${destDir}"
  cmd="find $sourceDir -type f -mtime +${modTime} -print0 | tar -czvf ${destDir}/logzilla-archive-$(date +%s).tgz --remove-files --null -T -"
  echo -e "${GREEN}COMMAND:${NC} $cmd\n"
  if ask "Ready to proceed?"; then
    mkdir -p ${destDir}
    find $sourceDir -type f -mtime +${modTime} -print0 | tar -czvf ${destDir}/logzilla-archive-$(date +%s).tgz --remove-files --null -T -
  fi
}

restore () {
  [[ -z "$tarFile" ]] && { echo -e "${RED}ERROR:${NC} Missing '-t' (source TAR file) for restore, e.g.:\n     $0 -r -t /mnt/foo/myfile.tgz\n"; exit 1; }
  [[ -z "$destDir" ]] && destDir="/"
  echo -e "\n${GREEN}RESTORING ARCHIVE:${NC} $tarFile to ${destDir}"
  cmd="tar xzvf $tarFile -C $destDir"
  echo -e "${GREEN}COMMAND:${NC} $cmd"
  echo -e "${GREEN}NOTE:${NC} backups contain full paths, so restoring to '/' is normal"
  echo -e "      If you are unsure, answer 'n' below and check the file using ${BLUE}tar tzvf $tarFile${NC}\n"
  if ask "Ready to proceed?"; then
    mkdir -p ${destDir}
    tar xzvf $tarFile -C $destDir
  fi
}

[[ $archive -eq 1 ]] && archive $sourceDir $destDir $modTime
[[ $restore -eq 1 ]] && restore $tarFile $destDir
