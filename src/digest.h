
#ifndef DIGEST_H
#define DIGEST_H

/*
	The DIGEST_NONCE_WINDOW defines the amount of time, in seconds, which
	a nonce is considered to be valid.
*/
#define DIGEST_NONCE_WINDOW		(15*60)


/* function prototypes for functions implemented in this module */
int get_digest(per_request *reqInfo, char *user, char *realm, char *digest, 
	       security_data* sec);
void Digest_Construct401(per_request *reqInfo, char *s, int stale, 
			 char* auth_name);
void Digest_Check(per_request *reqInfo, char *user, security_data* sec);

#endif /* !DIGEST_H */




