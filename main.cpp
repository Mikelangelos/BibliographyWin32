#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <Commctrl.h>
#include "resource.h"

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

#define APP_NAME "Bibliography"
#define APP_BUILD "v0.0.2.0" //NOTE:Remember to update this please

//------------------------------[     DEBUG STUFF    ]------------------------------
inline void
_debug_OutputStringf(char *format, ...) //NOTE: This is inefficient for strings that don't include format
{
   va_list args;
   va_start (args, format);
   char buf[64];
   vsprintf_s (buf, format, args);
   va_end (args);
   OutputDebugString(buf);
}
#ifdef _DEBUG
#define DebugOutputStringf(S, ...) _debug_OutputStringf(S, __VA_ARGS__)
#define DebugOutputString(S) OutputDebugString(S)
#else
#define DebugOutputStringf(S, ...)
#define DebugOutputString(S)
#endif
//----------------------------------------------------------------------------------

#define PM_UPDATEENTRY (WM_USER+10)

//---------------------------
global_variable WNDPROC DefaultGroupBoxProc;
//---------------------------

struct Book {
   char name[64];
   char author[64];
// char desc[256];
   u16 id;
   s16 page_total;
   u16 pageset_count;
   u16 page_count;	
   u16 *pages;
   u32 *pagesets;
   u64 ISBN;
};

//---------------------------
inline void swap_u16_in_buffer(u16 *buf, u32 a, u32 b)
{
   u32 tmp = buf[a];
   buf[a] = buf[b];
   buf[b] = tmp;
}

void quicksort_u16(u16 *buf, u32 low, u32 high)
{
   u16 last;

   if(low >= high)
      return;
   last = low;
   for(u32 i = last+1; i <= high; ++i) 
      if(buf[low] > buf[i]) {
	 swap_u16_in_buffer(buf, ++last, i);
      }
   
   swap_u16_in_buffer(buf, last, low);
   if(last)
      quicksort_u16(buf, low, last-1);
   quicksort_u16(buf, last+1, high);
}

void rebalance_entry(Book *book)
{
   //NOTE: I'm not sure if I should merge pages with themselves first or merge them in pagesets first and do the
   //page merging between them after that.
   u32 p_cnt,ps_cnt = p_cnt = 0;
   u16 from[1024], to[1024], pages[1024];
   bool32 flag;	//used throughout the function on different levels as a switch (on/off);

   //extract pages from book
   for (ps_cnt = 0; ps_cnt < book->pageset_count; ++ps_cnt) {
      from[ps_cnt] = LOWORD(book->pagesets[ps_cnt]);
      to[ps_cnt] = HIWORD(book->pagesets[ps_cnt]);
      
      if (from[ps_cnt] > to[ps_cnt]) { //ensure from < to
	 u16 tmp = from[ps_cnt];
	 from[ps_cnt] = to[ps_cnt];
	 to[ps_cnt] = tmp;
      }
   }
   
   for (p_cnt = 0; p_cnt < book->page_count; ++p_cnt) {
      pages[p_cnt] = book->pages[p_cnt];
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
	    } else if (from[i] < from[j] && to[i] < to[j] && to[i] >= from[j]) {	//partly in
	       to[i] = to[j];
	       from[j] = to[j] = 0;
	       flag = true; 
	       break;	    
	    } else if (from[j] < from[i] && to[j] < to[i] && to[j] >= from[i]) {	//partly out
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
   
   if (p_cnt != book->page_count) {
      HeapFree(GetProcessHeap(), NULL, book->pages);
      book->pages = (u16*)HeapAlloc(GetProcessHeap(), NULL, sizeof(u16)*p_cnt);
      for (u32 i = 0; i < p_cnt ; ++i)
	 book->pages[i] = pages[i];
      book->page_count = p_cnt;  
   }

   if (ps_cnt != book->pageset_count) {
      HeapFree(GetProcessHeap(), NULL, book->pagesets);
      book->pagesets = (u32*)HeapAlloc(GetProcessHeap(), NULL, sizeof(u32)*ps_cnt);
      for (u32 i = 0; i < ps_cnt ; ++i)
	 book->pagesets[i] = (u32)MAKELONG(from[i], to[i]);
      book->pageset_count = ps_cnt;  
   }
}


void update_entry(Book *book, u32 page_range)
{
   //TODO: implement seperate logic here. this is totaly inefficient.
   if (book->pageset_count) {
      //add security by checking the tmp
      u32 *tmp = (u32*)HeapReAlloc(GetProcessHeap(), NULL, book->pagesets, sizeof(u32)*(book->pageset_count+1));
      book->pagesets = tmp;
      book->pagesets[book->pageset_count++] = page_range;
      rebalance_entry(book);
      return;
   } else {
      book->pagesets = (u32*)HeapAlloc(GetProcessHeap(), NULL, sizeof(u32));
      book->pagesets[0] = page_range;
      book->pageset_count = 1;
      return;
   }
}

void get_pages_read(Book *book, char *wp, s32 size)
{
   char *first = wp;
   u32 page_num = 0;
   u32 pageset_low = 0;
   u32 pageset_high = 0;
   u32 p = 0,ps = 0;
   s32 bytes_written;

   //initialization
   if(p < book->page_count)
      page_num = book->pages[p];
   else
      page_num = -1;
   if(ps < book->pageset_count) {
      pageset_low = LOWORD(book->pagesets[ps]);
      pageset_high = HIWORD(book->pagesets[ps]);
   } else {
      pageset_high = -1;
   }
   
   //main body
   while(p < book->page_count || ps < book->pageset_count) {
      if(pageset_high < page_num) {	
	 bytes_written = sprintf_s(wp, size - (wp-first), "%d-%d,",pageset_low,pageset_high);
	 wp += bytes_written;
	 if(ps+1 < book->pageset_count) {
	    pageset_low = LOWORD(book->pagesets[++ps]);
	    pageset_high = HIWORD(book->pagesets[ps]);
	 } else {
	    pageset_high = -1;
	    ++ps;
	 }
      } else {
	 bytes_written = sprintf_s(wp, size - (wp-first),"%d,",page_num);
	 wp += bytes_written;
	 if(p+1 < book->page_count) {
	    page_num = book->pages[++p];
	 } else {
	    page_num = 0;
	    ++p;
	 }
      }
   }
   if(wp - first)
      *(wp-1) = '\0';
   else
      *wp = '\0';
   return;
}

u32 convert_u32_lowhigh(u32 input, u16 *low, u16 *high)
{
   if(input) {
      *low = (u16)(input & 0xFFFF);
      *high = (u16)(input >> 16);
      return (false);
   } else {
      u32 result = (u32)*high << 16 | (u32)*low;
      return (result);
   }
}

#if 1 // TODO: this function should be cleaned, maybe reworked, add security and be optimized/documented
u32 load_books_from_file(HANDLE file, Book **book_list)
{
   LARGE_INTEGER file_size;
   char *file_contents;			//NOTE:has to be freed at the end
   char string_buffer[64];		//parse buffer
   char *string_i;			//parse buffer iterator
   Book *book_entry;			//the book being created
   u32 book_entry_count = 0;
   u16 pageset_low, pageset_high;	//the pageset being created
   u16 pageset_count, page_count;
   u32 page_buffer[4096];
  
   GetFileSizeEx(file, &file_size); 
   file_contents = (char*)HeapAlloc(GetProcessHeap(), NULL, file_size.QuadPart);
   ReadFile(file, file_contents, file_size.QuadPart, NULL, NULL);   
   char *c = file_contents;

   while(true) {
      //
      if(*c++ != '{')
	 if(c - file_contents < file_size.QuadPart)
	    continue;
	 else
	    break;	//loop exits here
	 
      //new book entry found
      book_entry = (Book*)HeapAlloc(GetProcessHeap(), NULL, sizeof(Book));
      pageset_count = 0;
      page_count = 0;

      //::Name::
      string_i = book_entry->name;
      while(*++c != '}')
	 *string_i++ = *c;
      *string_i = '\0';

      //::Author::
      while(*++c != '{')
	 ;
      string_i = book_entry->author;
      while(*++c != '}')
	 *string_i++ = *c;
      *string_i = '\0';

      //::ISBN::
      while(*++c != '{')
	 ;
      string_i = string_buffer;
      while(*++c != '}')
	 *string_i++ = *c;
      *string_i = '\0';
      book_entry->ISBN = strtol(string_buffer, NULL, 10);
      
      //::Total Pages::
      while(*++c != '{')
	 ;
      string_i = string_buffer;
      while(*++c != '}')
	 *string_i++ = *c;
      *string_i = '\0';
      book_entry->page_total = strtol(string_buffer, NULL, 10);
      
      //::PageSets::
      //NOTE: Assumes !STRICTLY! {low:high,low:high}
      while(*++c != '{')
	 ;
      while(*++c != '}') {	//<--- used only for the first check after the '{'
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
	 
	 page_buffer[pageset_count++] = convert_u32_lowhigh(NULL, &pageset_low, &pageset_high);	 

	 if(*c == ',')
	    continue;
	 else
	    break;
      }

      if(pageset_count) {
	 book_entry->pagesets = (u32*)HeapAlloc(GetProcessHeap(), NULL, sizeof(u32)*pageset_count);
	 for(u32 i = 0; i < pageset_count; ++i) 
	    book_entry->pagesets[i] = page_buffer[i];
      } else
	 book_entry->pagesets = NULL;
	 
      book_entry->pageset_count = pageset_count;
      
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
	 book_entry->pages = (u16*)HeapAlloc(GetProcessHeap(), NULL, sizeof(u16));
	 for(u32 i = 0; i < page_count; ++i) 
	    book_entry->pages[i] = page_buffer[i];
      } else
	 book_entry->pages = NULL;

      book_entry->page_count = page_count;
      
      book_list[book_entry_count++] = book_entry;
      book_entry = NULL;
      
   }
   HeapFree(GetProcessHeap(), NULL, file_contents);
   return (book_entry_count);
}
#endif
LRESULT CALLBACK DetailsBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   LRESULT result = 0;

   
   local_persist HWND book_listview, book_groupbox, book_details_text, update_book_btn;
   local_persist s32 xChar, yChar, client_size_x, client_size_y;
   local_persist Book *book_list[256];
   local_persist u8 book_count;
   
   switch(msg) {
      case WM_CREATE: {
	 xChar = LOWORD(GetDialogBaseUnits());
	 yChar = HIWORD(GetDialogBaseUnits());

	 client_size_x = xChar * 70;
	 client_size_y = yChar * 25;
	 
	 SetWindowPos(hwnd, HWND_TOP,
		      ((CREATESTRUCT *)lParam)->x, ((CREATESTRUCT *)lParam)->y,
		      client_size_x, client_size_y, NULL);
		      
	 INITCOMMONCONTROLSEX icmnctrls = { sizeof(INITCOMMONCONTROLSEX),ICC_LISTVIEW_CLASSES };
	 InitCommonControlsEx(&icmnctrls);

	 book_listview = CreateWindow(WC_LISTVIEW, "",
				      WS_VISIBLE|WS_BORDER|WS_CHILD|LVS_REPORT|LVS_NOSORTHEADER|LVS_SINGLESEL|WS_TABSTOP,
				      xChar, yChar/2,
				      client_size_x - 4*xChar, 10*yChar,
				      hwnd, NULL,
				      ((CREATESTRUCT *)lParam)->hInstance, NULL);

	 ListView_SetExtendedListViewStyleEx(book_listview, 0, LVS_EX_FULLROWSELECT);

	 book_groupbox = CreateWindow("button", "Details:",
				      WS_VISIBLE|WS_CHILD|BS_GROUPBOX,
				      xChar, 21*yChar/2,
				      client_size_x - 4*xChar, client_size_y-29*yChar/2,
				      hwnd, (HMENU)IDG_DETAILS,
				      ((CREATESTRUCT *)lParam)->hInstance, NULL);

	 DefaultGroupBoxProc = (WNDPROC)SetWindowLong(book_groupbox,GWL_WNDPROC, (LONG)DetailsBoxProc);
	 
	 CreateWindow("static", "Name :\nAuthor :\nPages :\nPages Read :\n",
		      WS_VISIBLE|WS_CHILD,
		      xChar, yChar,
		      9*xChar, client_size_y-32*yChar/2,
		      book_groupbox, NULL,
		      ((CREATESTRUCT *)lParam)->hInstance, NULL);
	 
	 book_details_text = CreateWindow("static", "",
					  WS_VISIBLE|WS_CHILD,
					  10*xChar, yChar,
					  client_size_x - 24*xChar, client_size_y-32*yChar/2,
					  book_groupbox, NULL,
					  ((CREATESTRUCT *)lParam)->hInstance, NULL);
	 
	 update_book_btn =  CreateWindow("button","Update..",
					 WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_FLAT|WS_TABSTOP|WS_DISABLED,
					 client_size_x - 13*xChar, client_size_y-34*yChar/2,
					 8*xChar, 2*yChar,
					 book_groupbox, (HMENU)IDB_UPDATE,
					 ((CREATESTRUCT *)lParam)->hInstance, NULL);

	 CreateWindow("static", "",
		      WS_VISIBLE|WS_CHILD|SS_ETCHEDVERT,
		      client_size_x - 27*xChar/2-xChar/4, 5*yChar/8,
		      5, client_size_y-30*yChar/2-yChar/8,
		      book_groupbox, NULL,
		      ((CREATESTRUCT *)lParam)->hInstance, NULL);


	 
	 //TODO: Better optimize the following operations
	 //create columns
	 LVCOLUMN column;
	 column.mask = LVCF_FMT|LVCF_TEXT|LVCF_SUBITEM|LVCF_WIDTH;
	 column.fmt = LVCFMT_LEFT;

	 column.pszText = "Name";
	 column.iSubItem = 0;
	 column.cx = (client_size_x-4*xChar)*0.5f;
	 ListView_InsertColumn(book_listview, 0, &column);

	 column.pszText = "Author";
	 column.iSubItem = 1;
	 column.cx = (client_size_x-4*xChar)*0.35f;
	 ListView_InsertColumn(book_listview, 1, &column);

	 column.pszText = "Pages";
	 column.iSubItem = 2;
	 column.cx = (client_size_x-4*xChar)*0.15f;
	 column.fmt = LVCFMT_CENTER;	 
	 ListView_InsertColumn(book_listview, 2, &column);

	 //file io
	 HANDLE read_file = CreateFile("bibliography_list.txt",
				       GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	 if(read_file != INVALID_HANDLE_VALUE) {
	    book_count = (u8)load_books_from_file(read_file, book_list);
	    CloseHandle(read_file);
	 }
	 //fill items and subitems
	 LVITEM item;
	 item.mask = LVIF_TEXT;
	 item.iSubItem = 0;

	 for(u32 i = 0; i < book_count; ++i) {
	    item.iItem = i;
	    item.pszText = book_list[i]->name; 
	    ListView_InsertItem(book_listview, &item);
	 }

	 item.iSubItem = 1;
	 for(u32 i = 0; i < book_count; ++i) {
	    item.iItem = i;
	    item.pszText = book_list[i]->author; 
	    ListView_SetItem(book_listview, &item);
	 }

	 item.iSubItem = 2;
	 char tmp_buffer[8];
	 for(u32 i = 0; i < book_count; ++i) {
	    snprintf(tmp_buffer,8,"%d",book_list[i]->page_total);
	    item.iItem = i;
	    item.pszText = tmp_buffer; 
	    ListView_SetItem(book_listview, &item);
	 }	 
      }break;

      case WM_COMMAND: {
	 switch(LOWORD(wParam)) {
	    
	 }
	    
      }break;
	 
      case WM_NOTIFY: {
	 switch(((NMHDR*)lParam)->code) {
	    case LVN_ITEMCHANGED:{
	       NMLISTVIEW *info = (NMLISTVIEW*)lParam;
	       if(info->uNewState == 3) {
		  Book *book = book_list[info->iItem];	//assumes that id codes start from 0!
		  char text[2046], page_buffer[512];
		  get_pages_read(book, page_buffer, 512);
		  snprintf(text, 2046, "%s\n%s\n%d\n%s\n", book->name, book->author, book->page_total, page_buffer);
		  SetWindowText(book_details_text, text);
		  EnableWindow(update_book_btn, true);
	       } else if(info->uNewState == 0 && info->uOldState == 2) {
		  SetWindowText(book_details_text, "");
		  EnableWindow(update_book_btn, false);
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

      case PM_UPDATEENTRY: {
	 s32 book_index = ListView_GetNextItem(book_listview, -1, LVNI_SELECTED);	//returns -1 for failure!
	 Book *book = book_list[book_index];
	 update_entry(book, (u32)lParam);

	 char text[2046], page_buffer[512];
	 get_pages_read(book, page_buffer, 512);
	 snprintf(text, 2046, "%s\n%s\n%d\n%s\n", book->name, book->author, book->page_total, page_buffer);
	 SetWindowText(book_details_text, text);
      }break;

      case WM_SYSCOLORCHANGE :
	 InvalidateRect (hwnd, NULL, TRUE);
	 break;
      
      case WM_DESTROY:
	 PostQuitMessage(EXIT_SUCCESS);
	 break;
	 
      default:
	 result = DefWindowProc(hwnd, msg, wParam, lParam);
	 break;
   }
   return (result);
}
BOOL CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   switch(msg) {
      case WM_COMMAND:
	 if (LOWORD(wParam) == IDDB_OK) {
	    char tmp[8];
	    u16 from, to;

	    GetDlgItemText(hwnd, IDDC_FROM, tmp, 8);
	    from = strtol(tmp, NULL, 10);
	    GetDlgItemText(hwnd, IDDC_TO, tmp, 8);
	    to = strtol(tmp, NULL, 10);

	    if (from == to) {
	       MessageBox(NULL, "ERROR: The two fields can't be the same number!\n\Tip: Start from a previously read page.", "Wrong Input", MB_OK|MB_ICONERROR);
	       return true;
	    }
	    
	    if (from > to) {
	       u16 tmp = from;
	       from = to;
	       to = tmp;
	    }
	    SendMessage(GetParent(hwnd),PM_UPDATEENTRY, NULL, (LPARAM)MAKELONG(from, to));
	    EndDialog(hwnd, 1);
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
	 SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &params, 0);
	 SelectObject(dc, CreateFontIndirect(&params.lfMessageFont));
	 return(LRESULT)GetSysColorBrush(COLOR_WINDOW);
      }break;

      case WM_COMMAND: {
	 switch(LOWORD(wParam)) {
	    case IDB_UPDATE:
	       DialogBox(NULL, MAKEINTRESOURCE(IDD_UPDATE), GetParent(hwnd), DialogProc);
	       break;
	 }
      }break;
   }
   
   return CallWindowProc(DefaultGroupBoxProc, hwnd, msg, wParam, lParam);
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

   if(!RegisterClass(&main_class)) {
      MessageBox(NULL, "Could not register class!", "Startup Error!", MB_OK | MB_ICONERROR);
      return (EXIT_FAILURE);
   }

   CreateWindow(
      APP_NAME,
      APP_NAME,
      WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
      CW_USEDEFAULT,CW_USEDEFAULT,
      CW_USEDEFAULT,CW_USEDEFAULT,
      NULL,
      LoadMenu(main_instance, MAKEINTRESOURCE(IDR_MAIN_MENU)),
      main_instance,
      NULL
      );
   
   MSG msg;
   while(GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg); 
      DispatchMessage(&msg); 
   }
   
   return (msg.wParam);
}
