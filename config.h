#define KEYVALLEN 256    
        
    /*   删除左边的空格   */    
    char * l_trim(char * szOutput, const char *szInput);    
        
    /*   删除右边的空格   */    
    char *r_trim(char *szOutput, const char *szInput);    
        
    /*   删除两边的空格   */    
    char * a_trim(char * szOutput, const char * szInput);    
        
        
    int GetProfileString(char *profile, char *AppName, char *KeyName, char *KeyVal );    