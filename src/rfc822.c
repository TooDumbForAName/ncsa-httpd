/*rfc822.c -- utilities to make httpd do rfc822
  Copyright (C) 1994 Enterprise Integration Technologies Corp
  30-Aug-94 ekr
*/

/*A wrapper around getline to do rfc822 line unfolding*/
int ht_rfc822_getline(char *s,int n,int f,unsigned int timeout)
  {
    static char pb=0;
    int len;
    
    if(pb){
      if(n--){
        if(pb=='\n'){
          *s=pb='\0';
          return;
        }
        else
          *s++=pb;
      }
      else
        return(0);
    }
   
    while(!getline(s,n,f,timeout)){
      len=strlen(s);
      s+=len;      
      n-=len;
      if(n==1)     /*If we get here, we've failed to read a full line*/
        return(0);

      /*Here's where things get heinous. We're gonna read 1 character
        ahead with read(2)...McCool doesn't leave us any choice*/
      if(read(f,&pb,1)<=0){
        pb=0;
        return(0);
      }

      if(pb=='\r'){
        if(read(f,&pb,1)<=0){
          pb=0;
          return(0);
        }
      }
        
      if(pb==' '){ /*This is a folded line*/
        /*Trim off trailing whitespace*/
        while(isspace(*--s)){
          *s='\0';
          n++;
        }
        s++;
        n--;
        *s++=pb;
        pb=0;
      }
      else
        return(0);
    }
  }
