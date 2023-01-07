#include "lineTreatment.h"
#include "pipeline.h"
#include "redirection.h"


/*todo Pour éviter d'éventuels débordements (notamment lors de l'expansion des jokers),
 * on pourra limiter le nombre et la taille des arguments autorisés sur une ligne de commande slash :
 * #define MAX_ARGS_NUMBER 4096
 * #define MAX_ARGS_STRLEN 4096
 */

/**
 * returns a string representing the prefix of a word.
 * The boundary between the prefix and the radical is the position pos, not included.
 * @param pos position
 * @param asteriskString word
 * @return prefix word
 */
char * getPrefixe(unsigned long pos, char * asteriskString){
    char * prefixe = malloc(sizeof(char) * (pos + 1));
    testMalloc(prefixe);
    if(pos > 0){
        memcpy(prefixe,asteriskString,pos);
    }
    prefixe[pos] = '\0';
    return prefixe;
}

/**
 * returns a string representing the suffix of a word.
 * The boundary between the suffix and the radical is the position pos, included.
 * @param pos position
 * @param asteriskString word
 * @return prefix word
 */
char * getSuffixe(unsigned long pos, char * asteriskString){
    size_t len = strlen(asteriskString);
    size_t sub_len = len - pos;
    char *sub = malloc(sub_len + 1);
    testMalloc(sub);
    strncpy(sub, asteriskString + pos, sub_len);
    sub[sub_len] = '\0';
    return sub;
}

/**
 * returns the sub-string from the start position to the next "/" (or the end of the word).
 * @param start position
 * @param asteriskString
 * @return
 */
char * getAstefixe(int start, const char * asteriskString){
    int end = start;
    while(asteriskString[end] != '\0' && asteriskString[end] != '/' ) end++;
    char * asterisk = malloc(sizeof(char) * (end - start + 1));
    testMalloc(asterisk);
    memcpy(asterisk,asteriskString + start,end - start);
    asterisk[end - start] = '\0';
    return asterisk;
}


/**
 * checks that asterisk_string is a suffix of entry_name.
 * A character by character comparison from the end to the beginning.
 * @param entry_name
 * @param asterisk_string
 * @return 1 for true, 0 for false.
 */
int strstrSuffixe(char * entry_name, char * asterisk_string){
    if(strlen(entry_name) < strlen(asterisk_string)) return 0;
    int index = strlen(entry_name) - 1;
    for(int i = strlen(asterisk_string) - 1; i >= 0; i--){
        if(entry_name[index] != asterisk_string[i]) return 0;
        index--;
    }
    return 1;
}

/**
 * the position of the first ' * ' that is not a valid path
 * (A directory/file that contains ' * ' in its name).
 * returns -1 if there is none
 * @param asteriskString
 * @return -1 or pos
 */
int getAsteriskPos(char * asteriskString){
    for(int index = 0; index < strlen(asteriskString); index++){
        if(asteriskString[index] == '*'){
            //look for the position of the first ' / ' after ' * '
            int pos_slash = index;
            while(asteriskString[pos_slash] != '\0' && asteriskString[pos_slash] != '/' ) pos_slash++;

            // get all the path to the end of the word containing ' * ' detected
            // for example : a/b/c/*test/x/y -> a/b/c/*test
            char * pre_and_ast = getPrefixe(pos_slash, asteriskString);

            struct stat st;
            //check if it's a valid path, in this case, we return the index
            if(lstat(pre_and_ast,&st) != 0) {
                free(pre_and_ast);
                return index;
            }else{ // if not, we look for the following ' * '.
                free(pre_and_ast);
            }
        }
    }
    return -1;
}


/**
 * This function is used for the expansion of the jokers.
 * It checks that the word contains a ' * '.
 * If it does, it replaces this star with all possible entries
 * in the directory corresponding to the prefix (' . ' if no prefix)
 * and to the rest of the word after the ' *  '
 * (if * is followed by chars before the next ' / ' or the end of the word).
 *
 * After that, we recursively call the function on all the results obtained,
 * and we combine the results of these recursive calls in a char **
 * which represents a list of each of these possibilities.
 * The function ends when the prefix or the word after ' * ' (and both together)
 * no longer matches an existing path.
 * It also terminates when there is no ' * ' in the word studied.
 *
 * this function is detailed with comments within it.
 *
 * @param asteriskString
 * @return char ** tab which represent
 * all the possible paths that can replace a path containing one or more ' * '.
 */
char ** replaceAsterisk(char * asteriskString) {
    char ** res = malloc(sizeof(char *) * 1);
    res[0] = NULL;

    struct stat st;
    // we check that the path exists
    if(lstat(asteriskString,&st) == 0) {
        // if yes, we go to end
        goto endWithAsterisk;
    }

    int posAsterisk = getAsteriskPos(asteriskString);

    // if there is no asterisk and the path is not valid
    if (posAsterisk == -1){
        return res; //res (empty) is returned
    }

    // if the asterisk is not prefixed
    if((posAsterisk != 0 && asteriskString[posAsterisk - 1] != '/')) goto endWithAsterisk;


    /*we are looking for 3 char * corresponding to
    * the front part of ' * '
    * the part where there is ' * '(but without the *)
    * the part after ' * '
    */
    char *prefixe = getPrefixe(posAsterisk, asteriskString);
    char *asterisk = getAstefixe(posAsterisk + 1, asteriskString);
    unsigned long tailleSuf = strlen(prefixe) + strlen(asterisk) + 1;
    char *suffixe = getSuffixe(tailleSuf, asteriskString);

    //Now we will open the prefix directory
    DIR *dir = NULL;
    struct dirent *entry;
    if (strlen(prefixe) == 0) {
        dir = opendir(".");
    } else {
        dir = opendir(prefixe);
    }

    // We check that the directory has been opened.
    // If it is not opened, it means that the path is not valid
    if (dir == NULL) {
        free(prefixe);
        free(suffixe);
        free(asterisk);
        //res (empty) is returned
        return res;
    }

    // we browse the entries in the directory
    while ((entry = readdir(dir)) != NULL) {
        // if the file is '.', '..' or hidden
        if (entry->d_name[0] == '.') continue;
        // case where there is a continuation to the sub-string with asterisk (*/test for example)
        if (strlen(suffixe) != 0) {
            // in this case, if the file is NOT a directory, we go to the next one
            if (entry->d_type != DT_DIR && entry->d_type != DT_LNK) continue;
        }
        // at each match between the substring and entry, we realloc the array
        if (strstrSuffixe(entry->d_name, asterisk) == 1) {
            char *newString = malloc(sizeof(char) * (strlen(prefixe) + strlen(entry->d_name) + strlen(suffixe) + 1));
            testMalloc(newString);
            sprintf(newString, "%s%s%s", prefixe, entry->d_name, suffixe);
            char** tmp = copyStringArray(res);
            char** replace = replaceAsterisk(newString);
            freeArray(res);
            res = combine_char_array(tmp, replace);
            freeArray(replace);
            freeArray(tmp);
            free(newString);
        }
    }
    closedir(dir);

    free(prefixe);
    free(suffixe);
    free(asterisk);

    return res;

    // in case we have to return the path given as argument, alone and unchanged
    endWithAsterisk:
    freeArray(res);
    res = malloc(sizeof(char *) * 2);
    res[0] = malloc(sizeof(char) * strlen(asteriskString) + 1);
    strcpy(res[0], asteriskString);
    res[1] = NULL;
    test_Arg_Len(res);
    return res;

}
/**
 * the purpose of this function is to browse and
 * return all possible paths to replace ' ** '.
 * This function is recursive and calls replaceAsterisk.
 * It is a recursion that can make a large number of sub-calls.
 * To avoid any overflow, we limit the size of the result to 4096 char.
 * If this limit is exceeded, an error will occur and slash will be aborted.
 *
 * Add to path the name of all the directories it contains,
 * and then call replaceAsterisk with this newpath.
 * It is this function that will check if newpath is valid.
 * In this case it will return a char ** of all possible
 * replacements of the single ' * ' in newpath (if any).*
 *
 * In addition to calling replaceAsterisk,
 * this function will make a recursive call
 * with the parameters (newpath,suffix).
 *
 * Thus we go through the whole arborescence.
 *
 * @param path the path prefix of ' ** '
 * @param suffixe the suffix ("" if no suffix)
 * @return char ** tab which represent all the possible paths that can
 * replace a path containing ' ** ' and one or more ' * '.
 */
char ** doubleAsteriskParcours(char * path, char * suffixe){
    char **res = malloc(sizeof(char *) * 1);
    res[0] = NULL;

    // initial case where path is still empty
    if(strcmp(path,"") == 0){
        char * s = malloc(sizeof(char) * (strlen(suffixe) + 1));
        strcpy(s,suffixe);

        char ** temp = copyStringArray(res);
        char ** replaceFirst = replaceAsterisk(s);
        freeArray(res);
        res = combine_char_array(temp, replaceFirst);
        freeArray(replaceFirst);
        freeArray(temp);
        free(s);

    }


    DIR *dir = NULL;
    struct dirent *entry;
    if (strcmp(path, "") == 0) dir = opendir(".");
    else dir = opendir(path);
    //printf("opendir\n");

    // on parcourt le repertoire courant
    while ((entry = readdir(dir)) != NULL) {
        //printf("entry = %s\n",entry->d_name);


        // cas où l'entrée est '.', '..' ou cachée
        if (entry->d_name[0] == '.') continue;
        // cas où l'entrée est un lien symbolique
        if (entry->d_type == DT_LNK) continue;

        // cas où général

        //printf("entry = repertory\n");
        // on appelle replaceAsterisk avec path/completeSuffixe
        // si le chemin n'existe pas, cet appel renverra un char ** vide
        // sinon, il renverra les chemins possibles
        char * newString;

        //si il y a un chemin avant
        if(strcmp(path,"") != 0){
            // si il y a un suffixe
            if(strcmp(suffixe,"") != 0){
                newString = malloc(sizeof(char) * (strlen(path) + strlen(entry->d_name) + strlen(suffixe) + 3));
                testMalloc(newString);
                sprintf(newString, "%s/%s/%s", path, entry->d_name, suffixe);
            }else{
                //pas de suffixe
                newString = malloc(sizeof(char) * (strlen(path) + strlen(entry->d_name) + 3));
                testMalloc(newString);
                sprintf(newString, "%s/%s/", path, entry->d_name);
            }
        }else{
            // pas de chemin avant

            // si il y a un suffixe
            if(strcmp(suffixe,"") != 0){
                newString = malloc(sizeof(char) * (strlen(entry->d_name) + strlen(suffixe) + 2));
                testMalloc(newString);
                sprintf(newString, "%s/%s", entry->d_name, suffixe);
            }else{
                //pas de suffixe
                newString = malloc(sizeof(char) * (strlen(entry->d_name) + 2));
                testMalloc(newString);
                sprintf(newString, "%s/", entry->d_name);
            }
        }

        char ** tmp = copyStringArray(res);
        char ** replace = replaceAsterisk(newString);
        freeArray(res);
        res = combine_char_array(tmp, replace);
        freeArray(replace);
        freeArray(tmp);
        free(newString);
        //printf("sortie appel replaceAsterisk\n");

        // maintenant on fait l'appel récursif,
        // pour tester si un autre chemin n'existe pas plus loin dans l'arborescence
        if(entry->d_type == DT_DIR) {

            char *newPath;
            if (strcmp(path, "") == 0) {
                newPath = malloc(sizeof(char) * (strlen(entry->d_name) + 1));
                sprintf(newPath, "%s", entry->d_name);
            } else {
                newPath = malloc(sizeof(char) * (strlen(path) + strlen(entry->d_name) + 2));
                sprintf(newPath, "%s/%s", path, entry->d_name);
            }
            tmp = copyStringArray(res);
            replace = doubleAsteriskParcours(newPath, suffixe);
            freeArray(res);
            res = combine_char_array(tmp, replace);
            freeArray(replace);
            freeArray(tmp);
            free(newPath);
            //printf("sortie appel res doubleAsteriskParcours\n");
        }

    }
    closedir(dir);


    //print_char_double_ptr(res);
    test_Arg_Len(res);
    return res;
}

char ** replaceDoubleAsterisk(char * asteriskString){
    //printf("--------------------------------------- replaceDoubleAsterisk ------------------------------------------------\n");
    char ** res = malloc(sizeof(char *) * 1);
    res[0] = NULL;
    char * suffixe;
    if(strlen(asteriskString) > 3) suffixe = getSuffixe(3, asteriskString);
    else {
        suffixe = malloc(sizeof(char) * 1);
        suffixe[0] = '\0';
    }
    //printf("suffixe = %s\n",suffixe);
    char** tmp = copyStringArray(res);
    freeArray(res);
    char** replace = doubleAsteriskParcours("", suffixe);
    res = combine_char_array(tmp, replace);
    freeArray(tmp);
    freeArray(replace);
    free(suffixe);

    test_Arg_Len(res);
    return res;
}


char* supprimer_occurences_slash(const char *s){
    char *result = malloc(strlen(s) + 1);
    int isPreviousSlash = 0;
    int j = 0;
    for(int i = 0; s[i] != '\0'; i++) {
        if(s[i] == '/'){
            if(isPreviousSlash == 0){
                isPreviousSlash = 1;
                result[j] = s[i];
                j++;
            }
        }else{
            isPreviousSlash = 0;
            result[j] = s[i];
            j++;
        }


    }
    result[j] = '\0';
    return result;
}

/**
 * Compare a string to multiple redirection signs (<,>,>|,>>,2>,2>|,2>>)
 * @param str the string to compare to
 * @return boolean: 1=true 0=false
 */
int strcmp_redirections(char* str) {
    char *list[] = {"<", ">", ">|", ">>", "2>", "2>|", "2>>"};

    for (int i = 0; i < 7; ++i) {
        if (strcmp(list[i], str) == 0) {
            return 1;
        }
    }
    return 0;
}

size_t pipes_quantity(cmd_struct cmd){
    size_t pipes=0;
    for(int i=0; i<cmd.taille_array; ++i){
        if(strcmp(*(cmd.cmd_array+i),"|")==0){
            // handle the error where a pipe is the last argument
            if(i==cmd.taille_array-1){
                syntax_error=1;
                errorCode=2;
            }
            pipes++;
        }
    }
    return pipes;
}

/**
 * Creates a cmds_struct with the commands separated by the redirection symbols.
 * example : [0] "ls -l | wc -l" -> [0] "ls -l" [0] "|" [0] "wc -l"
 * with each [] a new array
 * @param cmd the struct with an array of strings of the command line
 * @return cmds_struct with the commands separated
 */
cmds_struct separate_pipes(cmd_struct cmd){
    cmds_struct res;
    res.taille_array=0;
    size_t pipes=pipes_quantity(cmd);
    cmd_struct* cmds_array=malloc(sizeof(cmd_struct)*(pipes+1));

    // count of the strings in cmd
    int string_count=0;
    // count of the strings on the last array of res
    int string_count_cmds=0;
    // index of the last copied string
    int size_tmp=0;
    while(string_count<cmd.taille_array){
        // when the current string is a redirection symbol
        if(strcmp(*(cmd.cmd_array+string_count),"|")==0){
            // if there's two pipes next to each other, put an empty string
            if(string_count==size_tmp){
                (cmds_array+res.taille_array)->cmd_array=malloc(sizeof(char*));
                *(cmds_array+res.taille_array)->cmd_array=malloc(sizeof(char));
                cmds_array[res.taille_array].cmd_array[0][0]='\0';
                (cmds_array+res.taille_array)->taille_array=1;
                syntax_error=1;
                errorCode=2;
            }
            // copy all strings before the symbol in one array
            else{
                (cmds_array+res.taille_array)->cmd_array=copyNStringArray((cmd.cmd_array+size_tmp),string_count_cmds);
                size_tmp+=string_count_cmds;
                (cmds_array+res.taille_array)->taille_array=string_count_cmds;
            }

            res.taille_array++;
            size_tmp+=1;
            string_count_cmds=0;
        }
        else{
            string_count_cmds++;
        }
        string_count++;
    }

    // copy either the last part of the string or the string without redirections
    (cmds_array+res.taille_array)->cmd_array=copyNStringArray((cmd.cmd_array+size_tmp),string_count_cmds);
    (cmds_array+res.taille_array)->taille_array=string_count_cmds;
    res.taille_array++;
    res.cmds_array=cmds_array;
    return res;
}

/**
 * Interprets the commands to call the corresponding functions.
 * @param liste struct for the command
 */
void interpreter(char* first_command,cmds_struct separated_list) {
    if(syntax_error==1){
        dprintf(STDERR_FILENO,"slash: syntax error\n");
    }
    else{
        if(separated_list.taille_array>1){
            handle_pipe(separated_list);
        }
        else if(strcmp(first_command,"cd")==0 || strcmp(first_command,"pwd")==0 || strcmp(first_command,"exit")==0){
            handle_redirection_intern(*separated_list.cmds_array);
        }
        else{
            handle_redirection_extern(*separated_list.cmds_array);
        }
    }
}

/**
 * Verify syntax for the standard input and output
 * @param cmds the list of commands to verify
 */
void verify_syntax(cmds_struct cmds){
    for(int i=0; i<cmds.taille_array; ++i){
        for(int j=0; j<cmds.cmds_array[i].taille_array; ++j){
            if((strcmp(cmds.cmds_array[i].cmd_array[j],">")==0
            || strcmp(cmds.cmds_array[i].cmd_array[j],">>")==0
            || strcmp(cmds.cmds_array[i].cmd_array[j],">|")==0)
            && i!=(cmds.taille_array-1)){
                syntax_error=1;
                errorCode=2;
                return;
            }
            if((strcmp(cmds.cmds_array[i].cmd_array[j],">")==0
                || strcmp(cmds.cmds_array[i].cmd_array[j],">>")==0
                || strcmp(cmds.cmds_array[i].cmd_array[j],">|")==0)
                && j==(cmds.cmds_array[i].taille_array-1)){
                syntax_error=1;
                errorCode=2;
                return;
            }
            if(strcmp(cmds.cmds_array[i].cmd_array[j],"<")==0 && i!=0){
                syntax_error=1;
                errorCode=2;
                return;
            }
        }
    }
}

void joker_expansion(cmd_struct liste){
    //printf("joker_expansion \n");
    //on commence par ajouter la commande dans le tableau args
    char ** args = malloc(sizeof(char *));
    *(args) = NULL;
    // on combine args avec le nouveau char ** représentant les chaines obtenues en remplaçant * dans un argument de la liste.
    for(int i = 0; i < liste.taille_array; i++){
        // la fonction combine_char_array renvoie un nouveau pointeur char ** (malloc à l'intérieur)
        // la fonction replaceAsterisk renvoie un char ** (malloc à l'intérieur).
        char * suppressed_slash = supprimer_occurences_slash(*(liste.cmd_array+i));
        char** replace;
        // si suppressed_slash commence par ** on appelle replaceDoubleAsterisk
        if(strlen(suppressed_slash) > 1 && suppressed_slash[0] == '*' && suppressed_slash[1] == '*'){
            replace = replaceDoubleAsterisk(suppressed_slash);
        }else{ // sinon on utilise replaceAsterisk
            replace = replaceAsterisk(suppressed_slash);
        }

        //si n'a aucun string, alors donner liste.cmd_array[i]
        if(replace[0] == NULL){
            freeArray(replace);
            replace = malloc(sizeof(char *) * 2);
            replace[0] = malloc(sizeof(char) * (strlen(suppressed_slash) + 1));
            strcpy(replace[0],suppressed_slash);
            replace[1] = NULL;
        }
        char** tmp = copyStringArray(args);
        freeArray(args);
        args = combine_char_array(tmp,replace);
        freeArray(replace);
        freeArray(tmp);
        free(suppressed_slash);
    }
    cmd_struct new_liste;
    size_t tailleArray = 0;
    while(args[tailleArray] != NULL) tailleArray ++;
    new_liste.taille_array = tailleArray;
    new_liste.cmd_array = args;

    test_Arg_Len(new_liste.cmd_array);

    cmds_struct separated_list=separate_pipes(new_liste);
    verify_syntax(separated_list);
    interpreter(*new_liste.cmd_array,separated_list);
    syntax_error=0;
    freeCmdsArray(separated_list);
    freeCmdArray(new_liste);
}


/***
 * Turns a line into a command structure.
 * @param ligne line from the prompt
 * @return struct cmd_struct
 */
cmd_struct lexer(char* ligne){
    char** cmd_array=malloc(sizeof(char*));
    testMalloc(cmd_array);
    char* token;
    size_t* taille_array_init= malloc(sizeof(size_t));
    testMalloc(taille_array_init);
    size_t taille_token;
    size_t taille_array=0;
    *taille_array_init=1;

    // take each string separated by a space and copy it into a list of strings
    token=strtok(ligne," ");
    do{
        taille_token=strlen(token);
        if(taille_token>=MAX_ARGS_NUMBER){
            perror("MAX_ARGS_NUMBER REACHED");
            exit(1);
        }

        cmd_array=checkArraySize(cmd_array,taille_array,taille_array_init);
        *(cmd_array+taille_array)=malloc(sizeof(char)*(taille_token+1));
        *(cmd_array+taille_array)=memcpy(*(cmd_array+taille_array),token,taille_token+1);
        taille_array++;
    }
    while((token = strtok(NULL, " ")));


    free(token);
    free(taille_array_init);
    cmd_struct cmdStruct = {.cmd_array=cmd_array, .taille_array=taille_array};
    return cmdStruct;
}
