#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <Commctrl.h>
#include "resource.h"
#include <Shlobj.h>

//---------------------------

#define global_variable static
#define local_persist static

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float r32;
typedef double r64;
typedef s32 bool32;

typedef wchar_t wchar;

#define APP_NAME L"Bibliography"

//------------------------------[     DEBUG STUFF    ]------------------------------
void get_last_error_message(wchar *text)
{
	
     wchar buf[128];
     u32 err = GetLastError();
     _snwprintf_s(buf, 128, L"%s\n\n\t(Error code: %d)", text, err);
     MessageBoxW(NULL, buf, L"Error", MB_OK|MB_ICONERROR);
}

void _debug_OutputStringf(char *format, ...) //NOTE: This is inefficient for strings that don't include format
{
     va_list args;
     va_start (args, format);
     char buf[64];
     vsprintf_s(buf, format, args);
     va_end(args);
     OutputDebugStringA(buf);
}
#ifdef _DEBUG
#define DebugOutputStringf(S, ...)	_debug_OutputStringf(S, __VA_ARGS__)
#define DebugOutputString(S)		OutputDebugString(S)
#else
#define DebugOutputStringf(S, ...)
#define DebugOutputString(S)
#endif
//----------------------------------------------------------------------------------

#define EL_OPERATION (WM_USER+10)

//---------------------------
global_variable WNDPROC DefaultGroupBoxProc;
//---------------------------
#define MAX_ENTRY_COUNT 256
#define MAX_BOOK_NAME 128
#define MAX_BOOK_AUTHOR 64
//---------------------------

struct Book {
     char name[MAX_BOOK_NAME];
     char author[MAX_BOOK_AUTHOR];;
     s16 total_pages;
     u16 pagesets_read_count;
     u16 pages_read_count;	
     u16 *pages_read;
     u32 *pagesets_read;
     u64 ISBN;
};

//---------------------------
void create_file_path(wchar *path, u32 size)
{
     *path = L'\0';
     wchar_t *documents_path;

     SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &documents_path);
     wcscat_s(path, MAX_PATH, documents_path);
     wcscat_s(path, MAX_PATH, L"\\Bilbiography");
     CreateDirectoryW(path, NULL);

     wcscat_s(path, MAX_PATH, L"\\bibliography_list.txt");
     CoTaskMemFree(documents_path); //has to free the pointer allocated&given to us by the system
     return;
}

//TODO: I should optimize this function.
//I can cache the path or create a struct to have all that info..??
void save_to_file(Book **entry_list, s32 total_entries, wchar *path)
{
     HANDLE wf = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

     if (wf == INVALID_HANDLE_VALUE) {
	  create_file_path(path, MAX_PATH);
	  wf = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	  if (wf == INVALID_HANDLE_VALUE) {
	       get_last_error_message(L"Can't open a write handle!");
	       return;
	  }
     }

     char buf[1024];
     u32 bytecnt;
     Book *book;
     DWORD bytes_written;

     for (u32 i = 0; i < total_entries; ++i) {
	  bytecnt = 0;
	  book = entry_list[i];

	  bytecnt += sprintf_s(buf, sizeof(buf)-bytecnt, "{{%s}\r\n", book->name);
	  bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "{%s}\r\n", book->author);
	  bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "{%llu}\r\n", book->ISBN);
	  bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "{%hd}\r\n", book->total_pages);

	  bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "{");
	  if (book->pagesets_read_count) {
	       for (u32 i = 0; i < book->pagesets_read_count; ++i) {
		    if (i+1 == book->pagesets_read_count )
			 bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "%d:%d}\r\n", LOWORD(book->pagesets_read[i]),HIWORD(book->pagesets_read[i]));
		    else
			 bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "%d:%d,", LOWORD(book->pagesets_read[i]),HIWORD(book->pagesets_read[i]));
	       }
	  } else {	 
	       bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "}\r\n");
	  }

	  bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "{");
	  if (book->pages_read_count) {
	       for (u32 i = 0; i < book->pages_read_count; ++i) {
		    if (i+1 == book->pages_read_count )
			 bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "%d}}\r\n\r\n", book->pages_read[i]);
		    else
			 bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "%d,", book->pages_read[i]);
	       }
	  } else {
	       bytecnt += sprintf_s(buf+bytecnt, sizeof(buf)-bytecnt, "}\r\n\r\n");
	  }
      
	  WriteFile(wf, buf, bytecnt, &bytes_written, NULL);
     }
     CloseHandle(wf);
}

inline void swap_in_two_u16_arrays(u16 *primary, u16 *slave, u32 a , u32 b)
{
     u32 tmpp = primary[a];
     u32 tmps = slave[a];
     primary[a] = primary[b];
     slave[a] = slave[b];
     primary[b] = tmpp;
     slave[b] = tmps;
}

inline void swap_in_u16_array(u16 *buf, u32 a, u32 b)
{
     u32 tmp = buf[a];
     buf[a] = buf[b];
     buf[b] = tmp;
}

void quicksort_two_u16(u16 *prime, u16 *slave, s32 low, s32 high)
{
     if (low >= high)
	  return;
     
     s32 pivot = low;

     for (u32 i = low+1; i <= high; ++i)
	  if (prime[low] > prime[i]) {
	       swap_in_two_u16_arrays(prime, slave, ++pivot, i);
	  }
     swap_in_two_u16_arrays(prime, slave, pivot, low);
     
     quicksort_two_u16(prime, slave, low, pivot-1);
     quicksort_two_u16(prime, slave, pivot+1, high);
}

void quicksort_u16(u16 *buf, s32 low, s32 high)
{
     s16 pivot;

     if(low >= high)
	  return;
     pivot = low;
     for(u32 i = pivot+1; i <= high; ++i) 
	  if(buf[low] > buf[i]) {
	       swap_in_u16_array(buf, ++pivot, i);
	  }
   
     swap_in_u16_array(buf, pivot, low);
     
     quicksort_u16(buf, low, pivot-1);
     quicksort_u16(buf, pivot+1, high);
}

void rebalance_entry(Book *book)
{
     //NOTE: I'm not sure if I should merge pages with themselves first or merge them in pagesets first and do the
     //page merging between them after that.
     u32 p_cnt,ps_cnt = p_cnt = 0;
     u16 from[1024], to[1024], pages[1024];
     bool32 flag;	//used throughout the function on different levels as a switch (on/off);

     //extract pages from book
     for (ps_cnt = 0; ps_cnt < book->pagesets_read_count; ++ps_cnt) {
	  from[ps_cnt] = LOWORD(book->pagesets_read[ps_cnt]);
	  to[ps_cnt] = HIWORD(book->pagesets_read[ps_cnt]);
      
	  if (from[ps_cnt] > to[ps_cnt]) { //ensure from < to
	       u16 tmp = from[ps_cnt];
	       from[ps_cnt] = to[ps_cnt];
	       to[ps_cnt] = tmp;
	  }
     }
   
     for (p_cnt = 0; p_cnt < book->pages_read_count; ++p_cnt) {
	  pages[p_cnt] = book->pages_read[p_cnt];
     }
   
     //check if pages can be combined between them
     //NOTE:inefficient cause it doesn't detect more that 1 consecutive page
     flag = false;
     if(p_cnt) {
	  quicksort_u16(pages, 0, p_cnt-1);
	  for (u32 i = 0; i < p_cnt; ++i) {
	       if (pages[i]+1 == pages[i+1]) {
		    from[ps_cnt] = pages[i];
		    to[ps_cnt++] = pages[i+1];
		    pages[i] = 0;
		    pages[++i] = 0;
		    flag = true;
	       }
	  }
   
	  if(flag) {	//done only if some pages merged
	       for(u32 max = p_cnt, p_cnt = 0, i = 0; i < max; ++i) {
		    if(pages[i] != 0)
			 pages[p_cnt++] = pages[i];
	       }
	  }

	  //check if pages can be combined with pagesets
	  flag = false;
	  for(u32 max = p_cnt, p_cnt = 0, i = 0; i < p_cnt; ++i) {
	       for(u32 j = 0; j < ps_cnt; ++j) {
		    if(pages[i] >= from[j] && pages[i] <= to[j]) {
			 pages[i] = 0;
			 flag = true;
			 break;
		    } else if (pages[i] == from[j]-1) {
			 pages[i] = 0;
			 --from[j];
			 flag = true;
			 break;
		    } else if (pages[i] == to[j]+1) {
			 pages[i] = 0;
			 ++to[j];	    
			 flag = true;
			 break;
		    }
	       }
	  }
   
	  if (flag) {	//done only if some pages merged
	       u32 tmp_cnt = 0;
	       for (u32 i = 0; i < p_cnt; ++i) {
		    if (pages[i] != 0)
			 pages[tmp_cnt++] = pages[i];
	       }
	       p_cnt = tmp_cnt;
	  }
     }

     if(ps_cnt) {
	  //check if pagesets can merge
	  quicksort_two_u16(from, to, 0, ps_cnt-1);  
	  flag = false;
	  for (u32 i = 0 ; i < ps_cnt; ++i) {
	       for (u32 j = 0; j < ps_cnt; ++j) {
		    if (i == j || from[i] == 0 && to[i] == 0 || from[j] == 0 && to[j] == 0)
			 continue;
	 
		    if (from[i] >= from[j] && to[i] <= to[j]) {				//all in
			 from[i] = to[i] = 0;
			 flag = true;
			 break;
		    } else if (from[i] <= from[j] && to[i] >= to[j]) {			//all out
			 from[j] = to[j] = 0;
			 flag = true;
			 break;
		    } else if (from[i] < from[j] && to[i] < to[j] && to[i] >= from[j]-1) {	//partly in
			 to[i] = to[j];
			 from[j] = to[j] = 0;
			 flag = true; 
			 break;
		    } else if (from[j] < from[i] && to[j] < to[i] && to[j] >= from[i]-1) {	//partly out
			 from[i] = from[j]; 
			 from[j] = to[j] = 0;
			 flag = true;
			 break;
		    }
	       }
	  }
   
	  if (flag) {
	       u32 tmp_cnt = 0;//done only if some pagesets merged
	       for (u32 i = 0; i < ps_cnt; ++i) {
		    if (from[i] != 0 && to[i] != 0) {
			 from[tmp_cnt] = from[i];
			 to[tmp_cnt++] = to[i];	    
		    }
	       }
	       ps_cnt = tmp_cnt;
	  }
     }
   
     if (p_cnt != book->pages_read_count) {
	  HeapFree(GetProcessHeap(), NULL, book->pages_read);
	  book->pages_read = (u16*)HeapAlloc(GetProcessHeap(), NULL, sizeof(u16)*p_cnt);
	  for (u32 i = 0; i < p_cnt ; ++i)
	       book->pages_read[i] = pages[i];
	  book->pages_read_count = p_cnt;  
     }


     if (ps_cnt != book->pagesets_read_count) {
	  HeapFree(GetProcessHeap(), NULL, book->pagesets_read);
	  book->pagesets_read = (u32*)HeapAlloc(GetProcessHeap(), NULL, sizeof(u32)*ps_cnt);
	  book->pagesets_read_count = ps_cnt;  
     }
     for (u32 i = 0; i < ps_cnt ; ++i)
	  book->pagesets_read[i] = (u32)MAKELONG(from[i], to[i]);     
}


void update_entry(Book *book, u32 pageset)
{
     //TODO: implement seperate logic here. this is totaly inefficient.
     if (book->pagesets_read_count) {
	  //add security by checking the tmp
	  u32 *tmp = (u32*)HeapReAlloc(GetProcessHeap(), NULL, book->pagesets_read, sizeof(u32)*(book->pagesets_read_count+1));
	  book->pagesets_read = tmp;
	  book->pagesets_read[book->pagesets_read_count++] = pageset;
	  rebalance_entry(book);
	  return;
     } else {
	  book->pagesets_read = (u32*)HeapAlloc(GetProcessHeap(), NULL, sizeof(u32));
	  book->pagesets_read[0] = pageset;
	  book->pagesets_read_count = 1;
	  return;
     }
}

void get_pages_read(Book *book, char *buf, s32 size)
{
     char *first = buf;
     u32 page_num = 0;
     u32 pageset_low = 0;
     u32 pageset_high = 0;
     u32 p = 0,ps = 0;
     s32 bytes_written;

     //initialization
     if(book->pages_read_count)
	  page_num = book->pages_read[p];
     else
	  page_num = -1;
     if(book->pagesets_read_count) {
	  pageset_low = LOWORD(book->pagesets_read[ps]);
	  pageset_high = HIWORD(book->pagesets_read[ps]);
     } else {
	  pageset_high = -1;
     }
   
     //main body
     while(p < book->pages_read_count || ps < book->pagesets_read_count) {
	  if(pageset_high < page_num) {	
	       bytes_written = sprintf_s(buf, size - (buf-first), "%d-%d,",pageset_low,pageset_high);
	       buf += bytes_written;
	       if(ps+1 < book->pagesets_read_count) {
		    pageset_low = LOWORD(book->pagesets_read[++ps]);
		    pageset_high = HIWORD(book->pagesets_read[ps]);
	       } else {
		    pageset_high = -1;
		    ++ps;
	       }
	  } else {
	       bytes_written = sprintf_s(buf, size - (buf-first),"%d,",page_num);
	       buf += bytes_written;
	       if(p+1 < book->pages_read_count) {
		    page_num = book->pages_read[++p];
	       } else {
		    page_num = 0;
		    ++p;
	       }
	  }
     }
     if(buf - first)
	  *(buf-1) = '\0';
     else
	  *buf = '\0';
     return;
}

void set_entry_info_on_label(HWND label, Book *entry)
{
     char text[2046], page_buffer[512];
     wchar wtext[2046];
     get_pages_read(entry, page_buffer, 512);
     snprintf(text, 2046, "%s\n%s\n%llu\n%hu\n%s\n", entry->name, entry->author,
	      entry->ISBN, entry->total_pages, page_buffer);
     MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 2046);
     SetWindowTextW(label, wtext);
     return;
}

void load_from_file(Book **entry_list , u8 *entry_count, wchar *path)
{
     LARGE_INTEGER file_size;
     char *file_contents;		//NOTE:has to be freed at the end
     char string_buffer[MAX_BOOK_NAME];		//parse buffer
     char *string_i;			//parse buffer iterator
     Book *new_entry;			//the book being created
     u16 pageset_low, pageset_high;	//the pageset being created
     u16 pageset_count, page_count;
     u32 page_buffer[4096];
     DWORD bytes_read;

     HANDLE file = CreateFileW(path, GENERIC_READ, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

     if (file == INVALID_HANDLE_VALUE) {
	  create_file_path(path, MAX_PATH);
	  file = CreateFileW(path, GENERIC_READ, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	  if (file == INVALID_HANDLE_VALUE) {
	       get_last_error_message(L"Can't open a read handle!");
	       return;
	  }
     }
	 
     GetFileSizeEx(file, &file_size); 
     file_contents = (char*)HeapAlloc(GetProcessHeap(), NULL, file_size.QuadPart);
     ReadFile(file, file_contents, file_size.QuadPart, &bytes_read, NULL);   
     char *c = file_contents;

     while(true) {
	  //
	  if(*c++ != '{')
	       if(c - file_contents < file_size.QuadPart)
		    continue;
	       else
		    break;	//loop exits here
	 
	  //new book entry found
	  new_entry = (Book*)HeapAlloc(GetProcessHeap(), NULL, sizeof(Book));
	  pageset_count = 0;
	  page_count = 0;

	  //::Name::
	  string_i = new_entry->name;
	  for (u32 i = 0; *++c != '}' && i < MAX_BOOK_NAME; ++i)
	       *string_i++ = *c;
	  *string_i = '\0';

	  //::Author::
	  while (*++c != '{')
	       ;
	  string_i = new_entry->author;
	  for (u32 i = 0; *++c != '}' && i < MAX_BOOK_AUTHOR; ++i)
	       *string_i++ = *c;
	  *string_i = '\0';

	  //::ISBN::
	  while (*++c != '{')
	       ;
	  string_i = string_buffer;
	  while (*++c != '}')
	       *string_i++ = *c;
	  *string_i = '\0';
	  new_entry->ISBN = strtoull(string_buffer, NULL, 10);
      
	  //::Total Pages::
	  while (*++c != '{')
	       ;
	  string_i = string_buffer;
	  while (*++c != '}')
	       *string_i++ = *c;
	  *string_i = '\0';
	  new_entry->total_pages = strtol(string_buffer, NULL, 10);
      
	  //::PageSets::
	  //NOTE: Assumes !STRICTLY! {low:high,low:high}
	  while (*++c != '{')
	       ;
	  while (*++c != '}') {	//<--- used only for the first check after the '{'
	       string_i = string_buffer;
	       while(*c != ':') 
		    *string_i++ = *c++;
	       *string_i = '\0';
	       pageset_low = strtol(string_buffer, NULL, 10);

	       string_i = string_buffer;
	       while(*++c != ',' && *c != '}') 
		    *string_i++ = *c;
	       *string_i = '\0';
	       pageset_high = strtol(string_buffer, NULL, 10);
	 
	       page_buffer[pageset_count++] = MAKELONG(pageset_low, pageset_high);	 

	       if(*c == ',')
		    continue;
	       else
		    break;
	  }

	  if(pageset_count) {
	       new_entry->pagesets_read = (u32*)HeapAlloc(GetProcessHeap(), NULL, sizeof(u32)*pageset_count);
	       for(u32 i = 0; i < pageset_count; ++i) 
		    new_entry->pagesets_read[i] = page_buffer[i];
	  } else
	       new_entry->pagesets_read = NULL;
	 
	  new_entry->pagesets_read_count = pageset_count;
      
	  //::Pages::
	  //NOTE: Assumes !STRICTLY! {page,page,page}
	  while(*++c != '{')
	       ;
	  while(*++c != '}') {
	       string_i = string_buffer;
	       while(*c != ',' && *c != '}') 
		    *string_i++ = *c++;
	       *string_i = '\0';
	       page_buffer[page_count++] = strtol(string_buffer, NULL, 10);

	       if(*c == ',')
		    continue;
	       else
		    break;
	  }
      
	  if(page_count) {
	       new_entry->pages_read = (u16*)HeapAlloc(GetProcessHeap(), NULL, sizeof(u16)*page_count);
	       for(u32 i = 0; i < page_count; ++i) 
		    new_entry->pages_read[i] = page_buffer[i];
	  } else
	       new_entry->pages_read = NULL;

	  new_entry->pages_read_count = page_count;
      
	  entry_list[(*entry_count)++] = new_entry;
	  new_entry = NULL;
      
     }
     HeapFree(GetProcessHeap(), NULL, file_contents);
     CloseHandle(file);
     return;
}

BOOL CALLBACK EditDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	local_persist Book *book;

	switch (msg)
	{
	case WM_INITDIALOG:
	     book = (Book*)lParam;
	     char tmp[11];
	     wchar wtmp[MAX_BOOK_NAME];

	     MultiByteToWideChar(CP_UTF8, 0, book->name, -1, wtmp, MAX_BOOK_NAME);
	     SetDlgItemTextW(hwnd, IDDC_NAME, wtmp);

	     MultiByteToWideChar(CP_UTF8, 0, book->author, -1, wtmp, MAX_BOOK_AUTHOR);
	     SetDlgItemTextW(hwnd, IDDC_AUTHOR, wtmp);

	     snprintf(tmp, 11, "%llu", book->ISBN);
	     MultiByteToWideChar(CP_UTF8, 0, tmp, -1, wtmp, MAX_BOOK_AUTHOR);
	     SetDlgItemTextW(hwnd, IDDC_ISBN, wtmp);

	     SetDlgItemInt(hwnd, IDDC_TOTALPAGES, book->total_pages, false);
	     return true;

	case WM_COMMAND:
	     if (LOWORD(wParam) == IDDB_OK)
	     {
		  char tmp[11];
		  wchar wtmp[MAX_BOOK_NAME];

		  GetDlgItemTextW(hwnd, IDDC_NAME, wtmp, MAX_BOOK_NAME);
		  WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, book->name, MAX_BOOK_NAME, NULL, NULL);
	       
		  GetDlgItemTextW(hwnd, IDDC_AUTHOR, wtmp, MAX_BOOK_AUTHOR);
		  WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, book->author, MAX_BOOK_AUTHOR, NULL, NULL);
	       
		  GetDlgItemTextW(hwnd, IDDC_ISBN, wtmp, 11);
		  WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, tmp, 10, NULL, NULL);
		  book->ISBN = strtoull(tmp, NULL, 10);

		  GetDlgItemTextW(hwnd, IDDC_TOTALPAGES, wtmp, 5);
		  WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, tmp, 10, NULL, NULL);
		  book->total_pages = strtol(tmp, NULL, 10);
		  
		  book = NULL;
		  EndDialog(hwnd, 0);
		  return true;
	     }
	     else if (LOWORD(wParam) == IDDB_CANCEL)
	     {
		  book = NULL;
		  EndDialog(hwnd, 0);
		  return true;
	     }
	}
	return false;
}

BOOL CALLBACK NewBookProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
     switch(msg) {
     case WM_COMMAND:
	  switch(LOWORD(wParam)) {
	  case IDDB_OK: {
	       //NOTE: Assuming that it has valid info
	       char tmp[11];
	       wchar wtmp[MAX_BOOK_NAME];
	       Book *new_book = (Book *)HeapAlloc(GetProcessHeap(), NULL, sizeof(Book));
	       
	       GetDlgItemTextW(hwnd, IDDC_NAME, wtmp, MAX_BOOK_NAME);
	       WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, new_book->name, MAX_BOOK_NAME, NULL, NULL);
	       
	       GetDlgItemTextW(hwnd, IDDC_AUTHOR, wtmp, MAX_BOOK_AUTHOR);
	       WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, new_book->author, MAX_BOOK_AUTHOR, NULL, NULL);
	       
	       GetDlgItemTextW(hwnd, IDDC_ISBN, wtmp, 11);
	       WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, tmp, 11, NULL, NULL);
	       new_book->ISBN = strtoull(tmp, NULL, 10);

	       GetDlgItemTextW(hwnd, IDDC_TOTALPAGES, wtmp, 5);
	       WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, tmp, 10, NULL, NULL);
	       new_book->total_pages = strtol(tmp, NULL, 10);
	       
	       SendMessageW(GetParent(hwnd), EL_OPERATION, 1, (LPARAM)new_book);
	       EndDialog(hwnd, 0);
	  }break;

	  case IDDB_CANCEL:
	       EndDialog(hwnd, 0);
	       break;
	  }
     }

     return false;
}

LRESULT CALLBACK DetailsBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
     LRESULT result = 0;

     local_persist HWND entry_lvw, details_grp, info_lbl, update_btn, delete_btn, edit_btn;
     local_persist s32 xchar, ychar, client_width, client_height, details_width, details_height;
     local_persist Book *entry_list[MAX_ENTRY_COUNT];
     local_persist u8 entry_count;
     local_persist wchar file_path[MAX_PATH];
   
     switch(msg) {
     case WM_CREATE: {
	  xchar = LOWORD(GetDialogBaseUnits());
	  ychar = HIWORD(GetDialogBaseUnits());

	  client_width = xchar * 80;
	  client_height = ychar * 25;

	  //set the desired client width/height
	  SetWindowPos(hwnd, HWND_TOP,
		       ((CREATESTRUCT *)lParam)->x, ((CREATESTRUCT *)lParam)->y,
		       client_width, client_height, NULL);
		      
	  INITCOMMONCONTROLSEX icmnctrls = {sizeof(INITCOMMONCONTROLSEX),ICC_LISTVIEW_CLASSES };
	  InitCommonControlsEx(&icmnctrls);

	  entry_lvw = CreateWindowW(WC_LISTVIEW, L"",
				    WS_VISIBLE|WS_BORDER|WS_CHILD|LVS_REPORT|LVS_NOSORTHEADER|LVS_SINGLESEL|WS_TABSTOP,
				    xchar, ychar/2,
				    client_width - 4*xchar, 10*ychar,
				    hwnd, NULL,
				    ((CREATESTRUCT *)lParam)->hInstance, NULL);

	  ListView_SetExtendedListViewStyleEx(entry_lvw, 0, LVS_EX_FULLROWSELECT);

	  details_grp = CreateWindowW(L"button", L"Details:",
				      WS_VISIBLE|WS_CHILD|BS_GROUPBOX,
				      xchar, 21*ychar/2,
				      client_width - 4*xchar, client_height-29*ychar/2,
				      hwnd, NULL,
				      ((CREATESTRUCT *)lParam)->hInstance, NULL);

	  DefaultGroupBoxProc = (WNDPROC)SetWindowLong(details_grp,GWL_WNDPROC, (LONG)DetailsBoxProc);
	  
	  RECT details_dim;
	  GetWindowRect(details_grp, &details_dim);
	  details_width = details_dim.right - details_dim.left;
	  details_height = details_dim.bottom - details_dim.top;
	  
	  CreateWindowW(L"static", L"Name :\nAuthor :\nISBN :\nTotal_Pages :\nPages_Read :\n",
			WS_VISIBLE|WS_CHILD,
			xchar, ychar,
			9*xchar, details_height-3*ychar/2,
			details_grp, NULL,
			((CREATESTRUCT *)lParam)->hInstance, NULL);
	 
	  info_lbl = CreateWindowW(L"static", L"",
				   WS_VISIBLE|WS_CHILD|SS_LEFTNOWORDWRAP,
				   10*xchar, ychar,
				   details_width - 21*xchar, details_height-3*ychar/2,
				   details_grp, NULL,
				   ((CREATESTRUCT *)lParam)->hInstance, NULL);
	 
	  update_btn =  CreateWindowW(L"button", L"Update..",
				      WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_FLAT|WS_TABSTOP|WS_DISABLED,
				      details_width - 9*xchar, ychar,
				      8*xchar, 2*ychar,
				      details_grp, (HMENU)IDD_UPDATE,
				      ((CREATESTRUCT *)lParam)->hInstance, NULL);

	  delete_btn =  CreateWindowW(L"button", L"Delete",
				      WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_FLAT|WS_TABSTOP|WS_DISABLED,
				      details_width - 9 * xchar, details_height - 5*ychar/2,
				      8 * xchar, 2 * ychar,
				      details_grp, (HMENU)IDB_DELETE,
				      ((CREATESTRUCT *)lParam)->hInstance, NULL);
	  edit_btn = CreateWindowW(L"button", L"Edit..",
				   WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_FLAT|WS_TABSTOP|WS_DISABLED,
				   details_width-9*xchar, 7*ychar/2,
				   8*xchar, 2*ychar,
				   details_grp, (HMENU)IDB_EDIT,
				   ((CREATESTRUCT *)lParam)->hInstance, NULL);

	  CreateWindowW(L"static", L"",
			WS_VISIBLE|WS_CHILD|SS_ETCHEDVERT,
			details_width - 10*xchar, 8,
			1, details_height-6,
			details_grp, NULL,
			((CREATESTRUCT *)lParam)->hInstance, NULL);

	  //create item/subitem columns
	  LVCOLUMN column;
	  column.mask = LVCF_FMT|LVCF_TEXT|LVCF_SUBITEM|LVCF_WIDTH;
	  column.fmt = LVCFMT_LEFT;

	  column.pszText = L"Name";
	  column.iSubItem = 0;
	  column.cx = (client_width-4*xchar)*0.5f;
	  ListView_InsertColumn(entry_lvw, 0, &column);

	  column.pszText = L"Author";
	  column.iSubItem = 1;
	  column.cx = (client_width-4*xchar)*0.35f;
	  ListView_InsertColumn(entry_lvw, 1, &column);

	  column.pszText = L"Pages";
	  column.iSubItem = 2;
	  column.cx = (client_width-4*xchar)*0.15f;
	  column.fmt = LVCFMT_CENTER;	 
	  ListView_InsertColumn(entry_lvw, 2, &column);

	  load_from_file(entry_list, &entry_count, file_path);

	  //fill items and subitems in listview
	  wchar wtmp[MAX_BOOK_NAME];
	  LVITEM item;
	  item.mask = LVIF_TEXT;
	  item.iSubItem = 0;

	  for (u32 i = 0; i < entry_count; ++i) {
	       item.iItem = i;
	       MultiByteToWideChar(CP_UTF8, 0, entry_list[i]->name, -1, wtmp, MAX_BOOK_NAME);
	       item.pszText = wtmp; 
	       ListView_InsertItem(entry_lvw, &item);
	  }

	  item.iSubItem = 1;
	  for (u32 i = 0; i < entry_count; ++i) {
	       item.iItem = i;
	       MultiByteToWideChar(CP_UTF8, 0, entry_list[i]->author, -1, wtmp, MAX_BOOK_NAME);
	       item.pszText = wtmp;
	       ListView_SetItem(entry_lvw, &item);
	  }

	  item.iSubItem = 2;
	  for (u32 i = 0; i < entry_count; ++i) {
	       _snwprintf_s(wtmp,8,L"%d",entry_list[i]->total_pages);
	       item.iItem = i;
	       item.pszText = wtmp; 
	       ListView_SetItem(entry_lvw, &item);
	  }	 
     }break;

     case WM_COMMAND: {
	  switch (LOWORD(wParam)) {
	  case ID_FILE_NEW_BOOK:
	       DialogBoxW(NULL, MAKEINTRESOURCE(IDD_NEWBOOK), hwnd, NewBookProc);
	       ListView_SetItemState(entry_lvw, entry_count-1, LVIS_FOCUSED|LVIS_SELECTED, LVIS_FOCUSED|LVIS_SELECTED);
	       SetFocus(entry_lvw);
	       break;
	  }
	    
     }break;
	 
     case WM_NOTIFY: {
	  switch(((NMHDR*)lParam)->code) {
	  case LVN_ITEMCHANGED:{
	       NMLISTVIEW *info = (NMLISTVIEW*)lParam;
	       if(info->uNewState == 3) {
		    Book *book = entry_list[info->iItem];	//assumes that id codes start from 0!
		    
		    set_entry_info_on_label(info_lbl, book);

		    EnableWindow(update_btn, true);
		    EnableWindow(edit_btn, true);
		    EnableWindow(delete_btn, true);
	       } else if(info->uNewState == 0 && info->uOldState == 2) {
		    SetWindowTextW(info_lbl, L"");
		    EnableWindow(update_btn, false);
		    EnableWindow(edit_btn, false);
		    EnableWindow(delete_btn, false);
	       }
	  }break;  
	  }
     }break;
	 
     case WM_CTLCOLORSTATIC: {
	  s32 menu = (s32)GetMenu((HWND)lParam);
	  HDC dc = (HDC) wParam;
	  SetBkMode (dc, TRANSPARENT);
	  SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
	  return(LRESULT)GetSysColorBrush(COLOR_WINDOW);
     }break;

     case EL_OPERATION: {
	  switch(wParam) {
	  case 0: {	//::UPDATE::
	       s32 book_index = ListView_GetNextItem(entry_lvw, -1, LVNI_SELECTED);	//returns -1 for failure!
	       Book *book = entry_list[book_index];
	       update_entry(book, (u32)lParam);
	       
	       set_entry_info_on_label(info_lbl, book);

	       save_to_file(entry_list, entry_count, file_path);
	  }break;
	       
	  case 1: {	//::CREATE::
	       if (entry_count+1 < MAX_ENTRY_COUNT) {
		    Book *new_book = (Book*)lParam;
		    wchar wtext[MAX_BOOK_NAME];

		    LVITEM item;
		    item.mask = LVIF_TEXT;
		    item.iItem = entry_count;

		    item.iSubItem = 0;
		    MultiByteToWideChar(CP_UTF8, 0, new_book->name, -1, wtext, 2046);
		    item.pszText = wtext;
		    ListView_InsertItem(entry_lvw, &item);

		    item.iSubItem = 1;
		    MultiByteToWideChar(CP_UTF8, 0, new_book->author, -1, wtext, 2046);
		    item.pszText = wtext;
		    ListView_SetItem(entry_lvw, &item);

		    item.iSubItem = 2;
		    _snwprintf_s(wtext, 8, L"%d", new_book->total_pages);
		    item.pszText = wtext;
		    ListView_SetItem(entry_lvw, &item);
		  		   
		    entry_list[entry_count++] = new_book;
		    
		    save_to_file(entry_list, entry_count, file_path);

	       } else {
		    MessageBoxW(NULL, L"Maximum book count reached! You can not create more book!", L"ERROR", MB_ICONERROR);
		    HeapFree(GetProcessHeap(), NULL, (void *)lParam);
	       }
	  }break;

	  case 2: {	//::DELETE::
	       s32 book_index = ListView_GetNextItem(entry_lvw, -1, LVNI_SELECTED);	//returns -1 for failure!
	       u8 result = MessageBoxW(hwnd, L"Are you sure you want to delete this entry?", L"WARNING",MB_OKCANCEL|MB_ICONWARNING|MB_DEFBUTTON2);
	       if (result == IDOK) {
		    ListView_DeleteItem(entry_lvw, book_index);
		    HeapFree(GetProcessHeap(), NULL, entry_list[book_index]);
		    for (u32 i = book_index; i < entry_count-1; ++i)
			 entry_list[i] = entry_list[i+1];
		    --entry_count;
		    
		    save_to_file(entry_list, entry_count, file_path);
	       }
	  }break;
	       
	  case 3: {	//::EDIT::
	       s32 index = ListView_GetNextItem(entry_lvw, -1, LVNI_SELECTED);
	       Book *entry = entry_list[index];
	       DialogBoxParamW(NULL, MAKEINTRESOURCE(IDD_NEWBOOK), hwnd, EditDialogProc,(LPARAM)entry);

	       char text[2046], page_buffer[512];
	       wchar wtext[2046];
	       get_pages_read(entry, page_buffer, 512);
	       snprintf(text, 2046, "%s\n%s\n%llu\n%hu\n%s\n", entry->name, entry->author,
			entry->ISBN, entry->total_pages, page_buffer);
	       MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 2046);
	       SetWindowTextW(info_lbl, wtext);

	       LVITEM item = {};
	       item.mask = LVIF_TEXT;
	       item.iSubItem = 0;

	       MultiByteToWideChar(CP_UTF8, 0, entry->name, -1, wtext, 2046);
	       ListView_SetItemText(entry_lvw, index, 0, wtext);

	       MultiByteToWideChar(CP_UTF8, 0, entry->author, -1, wtext, 2046);
	       ListView_SetItemText(entry_lvw, index, 1, wtext);
	       
	       snprintf(text, 2046, "%hd", entry->total_pages);
	       MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 2046);
	       ListView_SetItemText(entry_lvw, index, 2, wtext);
	       save_to_file(entry_list, entry_count, file_path);

	       SetFocus(entry_lvw);
	  }break;
	       
	  }
     }break;

     case WM_SYSCOLORCHANGE :
	  InvalidateRect (hwnd, NULL, TRUE);
	  break;
      
     case WM_DESTROY:
	  PostQuitMessage(EXIT_SUCCESS);
	  break;
	 
     default:
	  result = DefWindowProcW(hwnd, msg, wParam, lParam);
	  break;
     }
     return (result);
}

BOOL CALLBACK UpdateDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
     switch (msg) {
     case WM_COMMAND:
	  if (LOWORD(wParam) == IDDB_OK) {
	       char tmp[8];
	       wchar wtmp[8];
	       u16 from, to;
		   
	       GetDlgItemTextW(hwnd, IDDC_FROM, wtmp, 8);
	       WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, tmp, 8, NULL, NULL);
	       from = strtol(tmp, NULL, 10);

	       GetDlgItemTextW(hwnd, IDDC_TO, wtmp, 8);
	       WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, tmp, 8, NULL, NULL);
	       to = strtol(tmp, NULL, 10);


	       if (from == to) {
		    MessageBoxW(NULL, L"ERROR: The two fields can't be the same number!\n\Tip: Start from a previously read page.", L"Wrong Input", MB_OK|MB_ICONERROR);
		    return true;
	       }
	    
	       if (from > to) {
		    u16 tmp = from;
		    from = to;
		    to = tmp;
	       }
	       SendMessageW(GetParent(hwnd),EL_OPERATION, NULL, (LPARAM)MAKELONG(from, to));
	       EndDialog(hwnd, 0);
	       return true;
	  } else if (LOWORD(wParam) == IDDB_CANCEL) {
	       EndDialog(hwnd, 0);
	       return true;
	  }
     }
     return false;
}

LRESULT CALLBACK DetailsBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
     switch(msg) {
     case WM_CTLCOLORSTATIC: {
	  NONCLIENTMETRICS params;
	  params.cbSize = sizeof(NONCLIENTMETRICS);
	  s32 menu = (s32)GetMenu((HWND)lParam);
	  HDC dc = (HDC) wParam;
	  SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &params, 0);
	  SelectObject(dc, CreateFontIndirectW(&params.lfMessageFont));
	  return(LRESULT)GetSysColorBrush(COLOR_WINDOW);
     }break;

     case WM_COMMAND: {
	  switch(LOWORD(wParam)) {
	  case IDD_UPDATE:
	       DialogBoxW(NULL, MAKEINTRESOURCE(IDD_UPDATE), GetParent(hwnd), UpdateDialogProc);
	       break;
	  case IDB_DELETE:
	       SendMessageW(GetParent(hwnd), EL_OPERATION, 2, NULL);
	       break;
	  case IDB_EDIT:
	       SendMessageW(GetParent(hwnd), EL_OPERATION, 3, NULL);
	  }
     }break;
     }
   
     return CallWindowProcW(DefaultGroupBoxProc, hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE main_instance, HINSTANCE JuNk_prev_instance, LPSTR cmd_line, int cmd_show)
{

     WNDCLASS main_class = {};
     main_class.style = CS_HREDRAW | CS_VREDRAW;
     main_class.lpfnWndProc = WindowProc;
     main_class.hInstance = main_instance;
     main_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
     main_class.hCursor = LoadCursor(NULL, IDC_ARROW);
     main_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
     main_class.lpszMenuName = APP_NAME;
     main_class.lpszClassName = APP_NAME;

     if(!RegisterClassW(&main_class)) {
	  MessageBoxW(NULL, L"Could not register class!", L"Startup Error!", MB_OK | MB_ICONERROR);
	  return (EXIT_FAILURE);
     }

     CreateWindowW(
	  APP_NAME,
	  APP_NAME,
	  WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
	  CW_USEDEFAULT,CW_USEDEFAULT,
	  CW_USEDEFAULT,CW_USEDEFAULT,
	  NULL,
	  LoadMenuW(main_instance, MAKEINTRESOURCE(IDR_MAIN_MENU)),
	  main_instance,
	  NULL
	  );
   
     MSG msg;
     while(GetMessageW(&msg, NULL, 0, 0)) {
	  TranslateMessage(&msg); 
	  DispatchMessageW(&msg); 
     }
   
     return (msg.wParam);
}
