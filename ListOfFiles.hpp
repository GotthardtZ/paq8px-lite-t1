#ifndef PAQ8PX_LISTOFFILES_HPP
#define PAQ8PX_LISTOFFILES_HPP

class ListOfFiles {
private:
    typedef enum {
        IN_HEADER, FINISHED_A_FILENAME, FINISHED_A_LINE, PROCESSING_FILENAME
    } STATE;
    STATE state; //parsing state
    FileName basePath;
    String listOfFiles; //path/file list in first column, columns separated by tabs, rows separated by newlines, with header in 1st row
    Array<FileName *> names; //all file names parsed from listOfFiles
public:
    ListOfFiles() : state(IN_HEADER), names(0) {}

    ~ListOfFiles() {
      for( int i = 0; i < (int) names.size(); i++ )
        delete names[i];
    }

    void setBasePath(const char *s) {
      assert(basePath.strsize() == 0);
      basePath += s;
    }

    void addChar(char c) {
      if( c != EOF)
        listOfFiles += c;
      if( c == 10 || c == 13 || c == EOF) //got a newline
        state = FINISHED_A_LINE; //empty lines / extra newlines (cr, crlf or lf) are ignored
      else if( state == IN_HEADER )
        return; //ignore anything in header
      else if( c == TAB ) //got a tab
        state = FINISHED_A_FILENAME; //ignore the rest (other columns)
      else { // got a character
        if( state == FINISHED_A_FILENAME )
          return; //ignore the rest (other columns)
        if( state == FINISHED_A_LINE ) {
          names.pushBack(new FileName(basePath.c_str()));
          state = PROCESSING_FILENAME;
          if( c == '/' || c == '\\' )
            quit("For security reasons absolute paths are not allowed in the file list.");
          //TODO: prohibit parent folder references in path ('/../')
        }
        if( c == 0 || c == ':' || c == '?' || c == '*' ) {
          printf("\nIllegal character ('%c') in file list.", c);
          quit();
        }
        if( c == BADSLASH )
          c = GOODSLASH;
        (*names[names.size() - 1]) += c;
      }
    }

    int getCount() { return (int) names.size(); }

    const char *getfilename(int i) { return names[i]->c_str(); }

    String *getString() { return &listOfFiles; }
};

#endif //PAQ8PX_LISTOFFILES_HPP
