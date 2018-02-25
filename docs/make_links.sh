#!/bin/bash
##my_dir="$(dirname "$0")"
##. "$my_dir/stack.sh"
mdfd="$(dirname "$1")"
mdfsd=$(basename "$1")
mdfsd="${mdfsd%.*}"
#declare -a myqueue
myqueue[0]="${mdfsd}"
cp -p "$1" "${1}.bak"
mdfo="${1}"
mdfi="${1}.bak"
while IFS= read -r line; do
#    echo "line ${line}"
	while [[ "${line}" =~ \[([a-zA-Z0-9[:blank:]_-]+)\]\(([[:upper:].]+[^\)]*)?\) ]] 
	do
		g1=${BASH_REMATCH[1]}
		g2=${BASH_REMATCH[2]}
		page="$(tr "a-z " "A-Z-" <<< "$g1").md"
		anchor="#$(tr "A-Z " "a-z-" <<< "$g1")"
		link=${g2/%$anchor}
		link=${link/%$page}
		link=${link/#REF:}
		link=${link/#REFA:}
		if [[ "${line}" =~ ^([[:blank:]]*)\*[[:blank:]]+\[[a-zA-Z0-9[:blank:]_-]+\]\(([[:upper:].]+[^\)]*)?\) ]]; then
			size=${#BASH_REMATCH[1]}
			idx=$(($size/5))
			ptr=$(($idx+1))
#			echo "size $size idx $idx ptr $ptr"
			myqueue[$ptr]="${page%.md}"
			dir="${myqueue[0]}/"
			for (( c=1; c<=$idx; c++ )) 
			do  
   				dir="${dir}${myqueue[$c]}/"
			done
			link="${dir}${link}" 			
		fi
#		echo "link $link"
		if [ -z "$link" ] && grep -qi "^#\+ ${g1}" "$mdfi"; then
			g2="$anchor"
		elif [ -f "${mdfd}/${link}${page}" ]; then
			g2="${link}${page}"
		elif [ -f "${mdfd}/${link}" ] && grep -qi "^#\+ ${g1}" "${mdfd}/${link}"; then
			g2="${link}${anchor}"
		fi
		g2=$(sed -e 's/[\/&]/\\&/g' <<< "$g2")
#		echo "g2 $g2"
		re="[${g1}]]((${g2})"
#		echo "regex $re"
		line=$(sed -e "s/\[[a-zA-Z0-9_ -]\+\](\([A-Z.]\+[^)]*\)\?)/${re}/" <<< "$line")
#	   	echo "line1 ${line}"
	done
	sed 's/\]\]((/](/g' <<< "$line" 
#	echo $line | sed 's/\]\]\(\(/](/g' > $mdfo
done < "$mdfi" > "$mdfo"
