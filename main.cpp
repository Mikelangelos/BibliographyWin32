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

//---------------------------
global_variable WNDPROC DefaultGroupBoxProc;
//---------------------------

struct book {
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
u32 load_books_from_file(HANDLE file, book **book_list)
{
   LARGE_INTEGER file_size;
   char *file_contents;			//NOTE:has to be freed at the end
   char string_buffer[64];		//parse buffer
   char *string_i;			//parse buffer iterator
   book *book_entry;			//the book being created
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
      book_entry = (book*)HeapAlloc(GetProcessHeap(), NULL, sizeof(book));
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

   
   local_persist HWND book_listview, book_groupbox;
   local_persist s32 xChar, yChar, client_size_x, client_size_y;
   local_persist book *book_list[256];
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
				      WS_VISIBLE|WS_BORDER|WS_CHILD|LVS_REPORT,
				      xChar, yChar/2,
				      client_size_x - 4*xChar, 10*yChar,
				      hwnd, NULL,
				      ((CREATESTRUCT *)lParam)->hInstance, NULL);

	 book_groupbox = CreateWindow("button", "Details:",
				      WS_VISIBLE|WS_CHILD|BS_GROUPBOX,
				      xChar, 21*yChar/2,
				      client_size_x - 4*xChar, client_size_y-29*yChar/2,
				      hwnd, (HMENU)IDG_DETAILS,
				      ((CREATESTRUCT *)lParam)->hInstance, NULL);

	 DefaultGroupBoxProc = (WNDPROC)SetWindowLong(book_groupbox,GWL_WNDPROC, (LONG)DetailsBoxProc);
	 
	 CreateWindow("static", "Name:\t\nAuthor:",
		      WS_VISIBLE|WS_CHILD|SS_NOTIFY,
		      xChar, yChar,
		      client_size_x - 4*xChar, client_size_y-29*yChar/2,
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

	 column.pszText = "Page Count";
	 column.iSubItem = 2;
	 column.cx = (client_size_x-4*xChar)*0.15f;
	 column.fmt = LVCFMT_CENTER;	 
	 ListView_InsertColumn(book_listview, 2, &column);


	 HANDLE read_file = CreateFile("bibliography_list.txt",
				       GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	 book_count = (u8)load_books_from_file(read_file, book_list);
	 CloseHandle(read_file);
	 
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
	 
      case WM_CTLCOLORSTATIC: {
	 s32 menu = (s32)GetMenu((HWND)lParam);
	 HDC dc = (HDC) wParam;
	 SetBkMode (dc, TRANSPARENT);
	 return(LRESULT)GetSysColorBrush(COLOR_WINDOW);
      }
   
      
      case WM_DESTROY:
	 PostQuitMessage(EXIT_SUCCESS);
	 break;
	 
      default:
	 result = DefWindowProc(hwnd, msg, wParam, lParam);
	 break;
   }
   return (result);
}

LRESULT CALLBACK DetailsBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   switch(msg) {
      case WM_CTLCOLORSTATIC: {
	 s32 menu = (s32)GetMenu((HWND)lParam);
	 HDC dc = (HDC) wParam;
	 return(LRESULT)GetStockObject(NULL_BRUSH);
      }
	 
      default:
	 return CallWindowProc(DefaultGroupBoxProc, hwnd, msg, wParam, lParam);
   }
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
