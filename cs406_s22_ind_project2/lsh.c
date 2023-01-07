#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>


// forward declarations
/**
 * @brief decides whether the user wants interactiveMode or batchMode
 * 
 * @param argc number of commandline arguments
 * @param argv commandline arguments 
 */
void controller(int argc, char *argv[]);

/**
 * @brief runs a shell that allows user to type commands directly 
 * 
 */
void interactiveMode(); 

/**
 * @brief reads input from a batch file and executes the commands therein
 * 
 * @param argc number of commandline arguments
 * @param argv commandline arguments
 */
void batchMode(int argc, char*argv[]); 


/**
 * @brief processes the user specified command to run 
 * 
 * @param cmdArr 
 * @param cmdArrSize 
 * @param parallel 
 */
void processCmd(char** cmdArr, int cmdArrSize, bool parallel, int pidIndex); 


/**
 * @brief Set the Paths object 
 * 
 * @param cmdArr user input 
 * @param cmdArrSize number of input 
 */
void setPaths(char** cmdArr, int cmdArrSize); 

/**
 * @brief search for a valid path given a command
 * 
 * @param command to search for a path for 
 * @return -1 if error, 
 */
int searchPaths(char* command); 

/**
 * @brief fork the current process
 * 
 * @param cmdArr user input 
 * @param cmdArrSize size of user input
 * @param pathIndex valid path 
 * @param parallel whether or not command is parallel, determines whether or not
 */
void forkCall(char** cmdArr, int cmdArrSize, int pathIndex, bool parallel, int pidIndex); 

/**
 * @brief functionality for built in command cd 
 * 
 * @param cmdArr user input 
 */
void changeDir(char** cmdArr); 


/**
 * @brief checks if the user would like to direct output to a file 
 * 
 * @param cmdArr user input
 * @param cmdArrSize size of user input
 * @return int -1 for any errors, 0 for no redirect, 1 for valid redirect 
 */
int checkRedirect(char** cmdArr, int cmdArrSize); 

/**
 * @brief sets redirect to user specified file 
 * 
 * @param cmdArr user input
 * @param cmdArrSize size of user input 
 */
void redirect(char** cmdArr, int cmdArrSize); 


/**
 * @brief remove unecessary white space like tabs, spaces, etc.
 * 
 * @param str to clean up
 * @return char* cleaned up string 
 */
char* removeWhiteSpace(char* str);

/**
 * @brief create necessary white space for accurate parsing 
 * @param str to process 
 * @return char* with added white spaces
 */
char* createWhiteSpace(char* str); 


/**
 * @brief checks how many commands the user wants to run in parallel
 * 
 * @param cmdArr user input 
 * @param cmdArrSize size of user input 
 * @return int number of parallel commands 
 */
int checkParallel(char** cmdArr, int cmdArrSize); 

/**
 * @brief format the input array to be valid for paralell processing
 * @param cmdArr user input to parse 
 * @param cmdArrSize size of user input 
 * @param numPar number of parallel commands 
 * @return int* array containing index of paralell command followed by number of parameters+command. 
 */
int* processParallel(char** cmdArr, int cmdArrSize, int numPar); 
void executeParallel(char** cmdArr, int* multi, int numPar); 
void printCmdArr(char** cmdArr, int cmdArrSize); 

void cleanUp(); 

// globals 
char error_message[30] = "An error has occurred\n"; 
char** paths = NULL; 
int numPaths = 0; 

pid_t* pids = NULL; 



/**
 * @brief main, entry point
 * 
 * @param argc  number of commandline arguments
 * @param argv commandline arguments
 * @return int 0 if all is well 
 */
int main(int argc, char *argv[]) {
  
  controller(argc, argv); 
  return 0;
}


/**
 * @brief decides whether the user wants interactiveMode or batchMode
 *  based on number of commandline arguments 
 * @param argc number of commandline arguments
 * @param argv commandline arguments 
 */
void controller(int argc, char *argv[]){

  char* temp[2] = {"", "/bin"}; 
  setPaths(temp, 2); 
  // interactive mode 
  if(argc == 1){
    //printf("interactive mode\n"); 
    interactiveMode();  
    
  } else if(argc == 2){
    // batch mode 
    //printf("batch mode\n"); 
    batchMode(argc, argv); 
  } else {
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(1); 
  }

}

/**
 * @brief runs a shell that allows user to type commands directly 
 * parses input and formats it properly 
 * determines if parallel commands are needed
 */
void interactiveMode(){
  size_t bufferSize = 1024;
  char* cmd = calloc(bufferSize, sizeof(char)); 
  while(true){
    printf("lsh> "); 

    getline(&cmd, &bufferSize, stdin); 
      
    // set last character to '\0' to get rid of endl char 
    cmd[strlen(cmd)-1] = '\0'; 

    char* temp = createWhiteSpace(cmd); 
    free(cmd);
    cmd = temp; 

    temp = removeWhiteSpace(cmd);
    free(cmd); 
    cmd = temp;  
    
    //printf("***%s***\n", cmd); 
        
    int counter = 0; 
    char* found; 
    char* cmdCpy = strdup(cmd); 
        
    while( (found = strsep(&cmdCpy, " ")) != NULL){
      counter++; 
    }
    
        
    char** cmdArr = malloc((counter+1) * sizeof(char*)); 
    cmdArr[counter] = NULL; 

    int counter2 = 0; 
    while( (found = strsep(&cmd, " ")) != NULL){
      cmdArr[counter2] = strdup(found); 
      counter2++; 
    }
    free(cmdCpy); 
    

    int numPar = checkParallel(cmdArr, counter); 
    if(numPar == 1){
      //printCmdArr(cmdArr, counter); 
      //processCmd(cmdArr, counter); 
      processCmd(cmdArr, counter, false, 0); 
    } else {
      // it's a parallel command 
      int* parArr = processParallel(cmdArr, counter, numPar); 
     
      executeParallel(cmdArr, parArr, numPar); 

    }

   

  }


}


/**
 * @brief reads input from a batch file and executes the commands therein
 *  reads file line by line, parses and formats input 
 *  determines if parallel commands are needed 
 * 
 * @param argc number of commandline arguments
 * @param argv commandline arguments
 */
void batchMode(int argc, char* argv[]) {
  //fprintf(stderr, "batch mode"); 
  FILE *file = fopen(argv[1], "r"); 
  if(file == NULL){
    write(STDERR_FILENO, error_message, strlen(error_message)); 
    cleanUp(); 
    exit(1); 
  }

  char* buffer; 
  size_t buffersize = 0; 
  ssize_t len;
  char* cmd; 
  while((len = getline(&buffer, &buffersize, file)) > 0){
    cmd = buffer; 

    
    cmd[strlen(cmd)-1] = '\0'; 
    
    char* temp = createWhiteSpace(cmd); 
    cmd = removeWhiteSpace(temp); 
    free(temp); 


    // parse command 
    int counter = 0; 
    char* found; 
    char* cmdCpy = strdup(cmd); 
        
    while( (found = strsep(&cmdCpy, " ")) != NULL){
      counter++; 
    }
    free(cmdCpy); 

    char** cmdArr = malloc((counter+1) * sizeof(char*)); 
    cmdArr[counter] = NULL;  

    int counter2 = 0; 
    while( (found = strsep(&cmd, " ")) != NULL){
      cmdArr[counter2] = strdup(found); 
      counter2++; 
    }


    int numPar = checkParallel(cmdArr, counter); 
    if(numPar == 1){

      processCmd(cmdArr, counter, false, 0); 
    } else {
      // it's a parallel command 
      int* parArr = processParallel(cmdArr, counter, numPar); 
      executeParallel(cmdArr, parArr, numPar); 
    }
    
  }

}


/**
 * @brief processes the user specified command to run 
 *  determines whether or not command is a built in command 
 *  or something to fork 
 * 
 * @param cmdArr user input 
 * @param cmdArrSize size of user input
 * @param parallel bool, whether or not to run parllel commands 
 */
void processCmd(char** cmdArr, int cmdArrSize, bool parallel, int pidIndex){
  if(strcmp(cmdArr[0], "exit") == 0){
      if(cmdArrSize == 1){
        cleanUp(); 
        for(int i = 0; i < cmdArrSize; i++){
          free(cmdArr[i]);
        }
        free(cmdArr); 
        exit(0); 
      } else {

        write(STDERR_FILENO, error_message, strlen(error_message)); 
      }

    } else if(strcmp(cmdArr[0], "cd") == 0){
      if(cmdArrSize == 2){
        changeDir(cmdArr); 
      } else {
        
        write(STDERR_FILENO, error_message, strlen(error_message)); 
      }
    } else if (strcmp(cmdArr[0], "path") == 0){
      setPaths(cmdArr, cmdArrSize); 
    } else {
      int res = searchPaths(cmdArr[0]);  
      if(res == -1){
        
       write(STDERR_FILENO, error_message, strlen(error_message)); 
      } else if( res == -2){
        return; 
      } else {
        forkCall(cmdArr, cmdArrSize, res, parallel, pidIndex); 
     
      }
      
    }
  

}

/**
 * @brief Set the Paths object
 *  adds paths to the global paths array 
 * @param cmdArr user input 
 * @param cmdArrSize size of user input 
 */
void setPaths(char** cmdArr, int cmdArrSize){
  if(paths != NULL){
    // need to free, else don't need to free 
    for(int i = 0; i < numPaths; i++){
      free(paths[i]); 
    }
    free(paths); 
  } 

  numPaths = cmdArrSize - 1; 
  paths = malloc(numPaths* sizeof(char*)); 
  
  for(int i = 1; i < cmdArrSize; i++){
    
    if(strcmp(cmdArr[i], "/bin") != 0 && strcmp(cmdArr[i], "/usr/bin") != 0){
      if(cmdArr[i][0] != '/'){

         paths[i-1] = calloc(1024, sizeof(char)); 
         // will have to change this depending on what directory it is in !!!
         char* cwd = getcwd(paths[i-1], 1024); 
         strcat(cwd, "/"); 
         //printf("cwd: %s\n", cwd);
         //strcat(paths[i-1], "/home/liucl/projects/project2/cs406_s22_ind_project2/"); 
      } else {
      }
      
      strcat(paths[i-1], cmdArr[i]); 
     // printf("%s\n", paths[i-1]);
    }
 
    else {
      paths[i-1] = strdup(cmdArr[i]); 
    }
    
  }
  
  /*
  for(int i = 0; i < numPaths; i++){
    printf("*%s*", paths[i]); 
  }
  printf("\n");
  */ 


}

/**
 * @brief search for a valid path given a command
 * 
 * @param command to search for a path for 
 * @return int index of path if valid, -2 if empty input, -1 for other errors,
 */
int searchPaths(char* command){
  // get rid of empty input s
  if(strcmp(command, "") == 0){
    return -2; 
  }

  for(int i = 0; i < numPaths; i++){
    char* temp = calloc(1028, sizeof(char)); 
    strcpy(temp, paths[i]); 
    strcat(temp, "/"); 
    strcat(temp, command);

    int res = access(temp, X_OK);
   
    if(res != -1){
      free(temp); 
      return i; 
    }
    free(temp); // is this necessary? maybe... 
  }
  return -1; 
}


/**
 * @brief fork the current process
 * check for redirection 
 * set the correct path 
 * 
 * @param cmdArr user input 
 * @param cmdArrSize size of user input
 * @param pathIndex valid path 
 * @param parallel whether or not command is parallel, determines whether or not
 * parent should wait here or in execParallel
 */
void forkCall(char** cmdArr, int cmdArrSize, int pathIndex, bool parallel, int pidIndex){
  int rc = fork(); 
  if(rc < 0){
          // fork failed 
    
    write(STDERR_FILENO, error_message, strlen(error_message)); 
         

  } else if (rc == 0){
      // child process 
      int checkRedirRes = checkRedirect(cmdArr, cmdArrSize); 
      if(checkRedirRes == 1){
        redirect(cmdArr, cmdArrSize); 
      } else if (checkRedirRes == -1){
        write(STDERR_FILENO, error_message, strlen(error_message)); 
        return; 
      }
      char* tempPath = calloc(1024, sizeof(char)); 
      
      
      strcpy(tempPath, "PATH=/bin:"); 
      strcat(tempPath, paths[pathIndex]); 
    

      putenv(tempPath); 
      
      execvp(cmdArr[0], cmdArr); 
  
      return; 
    } else {

      // parent process 
      if(!parallel){
        int rc_wait = wait(NULL); 
      } else {
        pids[pidIndex] = rc;
      }
      
    }


}


/**
 * @brief functionality for built in command cd 
 * 
 * @param cmdArr user input 
 */
void changeDir(char** cmdArr){
  int res = chdir(cmdArr[1]); 

  // not sure if need error message here yet.. 
  if(res == -1){
    //fprintf(STDERR_FILENO, error_message); 
    write(STDERR_FILENO, error_message, strlen(error_message)); 
  } 

}

/**
 * @brief checks if the user would like to direct output to a file 
 * 
 * @param cmdArr user input
 * @param cmdArrSize size of user input
 * @return int -1 for any errors, 0 for no redirect, 1 for valid redirect 
 */
int checkRedirect(char** cmdArr, int cmdArrSize){
  // errors: 
  // too many >
  // too many files specified 
  // no command specified before > 
  // no file specified 

  int counter = 0; // keeps track of how many > 
  bool penultimate = false; 
 
  for(int i = 0; i < cmdArrSize; i++){
    //strstr since > might be within a string and not in it's own element 
    if(strcmp(cmdArr[i], ">") == 0){
      
      // need to keep track of index found mostly here... 
      if(i == 0){ // no command specified before > 
        return -1; 
      }
      if(i == cmdArrSize-1){
        return -1; // no file specified, > was the last element 
      }
      if(i == cmdArrSize-2){
        penultimate = true; // tracks if > was found in penultimate position
      }
      counter++; 
      cmdArr[i] = NULL; 
    }
    
  }

  if(counter == 1){
    if(penultimate == true){
      return 1; // correct formatting 
    } else {
      return -1; // incorrect position, so potentially too many files specified 
    }
    return 1; 
  } else if (counter > 1){
    return -1; // too many ">"s
  } 
  return 0; 
}

/**
 * @brief sets redirect to user specified file 
 * redirects stdout and stderr ot that file 
 * 
 * @param cmdArr user input
 * @param cmdArrSize size of user input 
 */
void redirect(char** cmdArr, int cmdArrSize) {
  // redirect the file here
  close(STDOUT_FILENO); 
  close(STDERR_FILENO); 
  open(cmdArr[cmdArrSize-1], O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
      
  
}

/**
 * @brief remove unecessary white space like tabs, spaces, etc.
 * should be called after createWhiteSpace
 * 
 * @param str to clean up
 * @return char* cleaned up string 
 */
char* removeWhiteSpace(char* str){
  // just copy to a new string and reassign 
  char* newStr = calloc(strlen(str)+1, sizeof(char)); 
  bool prevIsWhite = false; 
  bool seen = false; 
  int currIndex = 0; 
  for(int i = 0; i < strlen(str); i++){
    if(isspace(str[i]) == 0){
      // NOT white space 
      newStr[currIndex] = str[i]; 
      prevIsWhite = false; 
      currIndex++; 
      seen = true; 
    } else {
      // is white space 
      if(prevIsWhite == false && seen == true){
        newStr[currIndex] = ' '; // make the white space into a space as it is the specified delimiter 
        prevIsWhite = true; 
        currIndex++; 
      }

    }
   
  }

  // take care of the final white space character if any
  if(isspace(newStr[strlen(newStr)-1]) != 0){
    newStr[strlen(newStr)-1] = '\0'; 
  }

  return newStr; 
  
}


/**
 * @brief create necessary white space for accurate parsing 
 * should be called before removeWhiteSpace
 * one space between each word/command/flag, no space at the end of input 
 * adds spaces for > and & 
 * @param str to process 
 * @return char* with added white spaces
 */
char* createWhiteSpace(char* str){
  char* newStr = calloc(strlen(str) + 20, sizeof(char));
  int currIndex = 0; 

  for(int i = 0; i < strlen(str); i++){
    // specifically just looking for > to insert white space 
    if(str[i] == '>' || str[i] == '&'){
      newStr[currIndex] = ' ';
      currIndex++;
      newStr[currIndex] = str[i]; // either > or & 
      currIndex++; 
      newStr[currIndex] = ' '; 
      currIndex++; 

    } else {
      // just copy in the last char 
      newStr[currIndex] = str[i]; 
      currIndex++; 
    }
  } 
  
  return(newStr); 
}

/**
 * @brief checks how many commands the user wants to run in parallel
 * 
 * @param cmdArr user input 
 * @param cmdArrSize size of user input 
 * @return int number of parallel commands 
 */
int checkParallel(char** cmdArr, int cmdArrSize){
  int counter = 0; 
  for(int i = 0; i < cmdArrSize; i++){
    if(strcmp(cmdArr[i], "&") == 0) {
      counter++; 
    }
  }
  return counter+1; 

}


/**
 * @brief format the input array to be valid for paralell processing
 * gets rid of & and replaces them with NULLs 
 * figures out indices of where commands start and how long the command is 
 * 
 * @param cmdArr user input to parse 
 * @param cmdArrSize size of user input 
 * @param numPar number of parallel commands 
 * @return int* array containing index of paralell command followed by number of parameters+command. 
 * alternating pattern between the two 
 */
int* processParallel(char** cmdArr, int cmdArrSize, int numPar){
  int* multi = calloc(numPar*2, sizeof(int)); // every other will be the index then the number of arguments something has 
  int multiIndex = 0; 
  bool next = false; 
  int counter = 0; 
  for(int i = 0; i < cmdArrSize; i++){
    
    if(strcmp(cmdArr[i], "&") == 0){
      multi[multiIndex] = counter; 
      multiIndex++; 
      counter = 0; 
      cmdArr[i] = NULL; 
      next = true; 
    } else if (next == true || i == 0){
      multi[multiIndex] = i; 
      multiIndex++; 
      next = false; 
      counter++; 
    } else {
      counter++; 
    }
 
  }
  multi[multiIndex] = counter; 

  // don't forget about the last one 
  return multi; 
}



/**
 * @brief execute the parallel commands 
 * 
 * @param cmdArr parallel commands to execute 
 * @param multi array of indices/sizes of input commands 
 * @param numPar number of parallel commands to run 
 */
void executeParallel(char** cmdArr, int* multi, int numPar){
  pids = calloc(numPar, sizeof(pid_t)); 
  
  int goal = 0; 

  for(int i = 0; i < numPar*2; i++){
    if(multi[i+1] != 0){
      processCmd(&cmdArr[multi[i]], multi[i+1], true, i); 
      goal++; 
    }
    
    i++; 
    
  }
  
  
  // the parent waits for all the children to finish 
  int counter = 0; 
  for(int i = 0; i < numPar; i++){
    pid_t rc = wait(NULL); 
    /*
    //waitpid(pids[i], NULL, 0); 
    pid_t rc = wait(NULL); 
    for(int j = 0; j < numPar; j++){
      if(rc == pids[j]){
        counter++; 
      }
    }
    */
  }
  //printf("counter: %d, goal: %d\n", counter, goal); 

  free(pids); 
}


/**
 * @brief utility function to print contents of user input array
 * 
 * @param cmdArr user input
 * @param cmdArrSize size of user input 
 */
void printCmdArr(char** cmdArr, int cmdArrSize){
  for(int i = 0; i < cmdArrSize; i++){
    char* toPrint = cmdArr[i]; 
    if(cmdArr[i] == NULL){
      toPrint = "NULL"; 
    }
    printf("**%s**", toPrint); 
  }
  printf("\n"); 
}


/**
 * @brief cleans up memory before program exit 
 * 
 */
void cleanUp(){
  if(paths != NULL){
     for(int i = 0; i < numPaths; i++){
    free(paths[i]);
  }
  free(paths); 

  }
  
 
}