/***************************************************************************
                    ckdevelop.cpp - the main class in CKDevelop
                             -------------------                                         

    begin                : 20 Jul 1998                                        
    copyright            : (C) 1998 by Sandy Meier                         
    email                : smeier@rz.uni-potsdam.de                                     
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/

#include <iostream.h>

#include <qclipbrd.h>
#include <qevent.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qfont.h>
#include <qregexp.h>
#include <qtextstream.h>
#include <qtoolbar.h>
#include <qmessagebox.h>

#include <kcursor.h>
#include <kfiledialog.h>
#include <kkeydialog.h>
#include <kmsgbox.h>
#include <ktabctl.h>

#include "./kdlgedit/kdlgedit.h"
#include "ctoolsconfigdlg.h"
#include "ktipofday.h"

// #include <kio_linedit_dlg.h>

#include "ckdevelop.h"
#include "ckdevsetupdlg.h"
#include "cupdatekdedocdlg.h"
#include "ccreatedocdatabasedlg.h"
#include "ctoolclass.h"
#include "cdocbrowser.h"
#include "doctreeview.h"
#include "cfinddoctextdlg.h"
#include "cexecuteargdlg.h"
#include "debug.h"
#include "./kwrite/kwdoc.h"
#include "kswallow.h"
#include "cerrormessageparser.h"
#include "grepdialog.h"
#include "cbugreportdlg.h"
#include "../config.h"
#include "structdef.h"
#include "vc/versioncontrol.h"


///////////////////////////////////////////////////////////////////////////////////////
// FILE-Menu slots
///////////////////////////////////////////////////////////////////////////////////////

void CKDevelop::slotFileNew(){
  
  slotStatusMsg(i18n("Creating new file..."));
  newFile(false);
  slotStatusMsg(i18n("Ready."));
}


void CKDevelop::slotFileOpen(){
  slotStatusMsg(i18n("Opening file..."));

  QString str;
  if(project){
    str = KFileDialog::getOpenFileName(prj->getProjectDir(),"*",this);
  }
  else{
    str = KFileDialog::getOpenFileName(0,"*",this);
  }  
  if (!str.isEmpty()) // nocancel
  {
    switchToFile(str);
  }

  slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotFileOpen( int id_ ){
  slotStatusMsg(i18n("Opening file..."));

  QString str=file_open_list.at(id_);
	
  switchToFile(str);

  slotStatusMsg(i18n("Ready."));

}

void CKDevelop::slotFileClose(){
    slotStatusMsg(i18n("Closing file..."));
    QString filename = edit_widget->getName();
    int message_result;

    if(edit_widget->isModified()){
	
	// no autosave if the user intends to save a file
	if (bAutosave)
	    saveTimer->stop();
	
	message_result = KMsgBox::yesNoCancel(this,i18n("Save?"),
					      i18n("The document was modified,save?"),KMsgBox::QUESTION);
	// restart autosaving
	if (bAutosave)
	    saveTimer->start(saveTimeout);
	
	if (message_result == 1){ // yes
	
	    if (isUntitled(filename))
		{
		    if (!fileSaveAs())
			message_result=3;    // simulate here cancel because fileSaveAs failed....
		}
	    else
		{
		
		    saveFileFromTheCurrentEditWidget();
		    if (edit_widget->isModified())
			message_result=3;		   // simulate cancel because doSave went wrong!
		}
	}
	
	if (message_result == 3) // cancel
	    {
		setInfoModified(filename, edit_widget->isModified());
		slotStatusMsg(i18n("Ready."));
		return;
	    }
    }

    removeFileFromEditlist(filename);
    setMainCaption();
    slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotFileCloseAll()
{
  slotStatusMsg(i18n("Closing all files..."));
  TEditInfo* actual_info;
  QStrList handledNames;
  bool cont=true;

  setInfoModified(header_widget->getName(), header_widget->isModified());
  setInfoModified(cpp_widget->getName(), cpp_widget->isModified());

  for(actual_info=edit_infos.first();cont && actual_info != 0;)
  {
    TEditInfo *next_info=edit_infos.next();
    if(actual_info->modified && handledNames.contains(actual_info->filename)<1)
    {
      KMsgBox *files_close=new KMsgBox(this,i18n("Save changed files ?"),
		    	   i18n("The project\n\n")+prj->getProjectName()
					   +i18n("\n\ncontains changed files. Save modified file\n\n")
					   +actual_info->filename+" ?\n\n",KMsgBox::QUESTION,
					   i18n("Yes"), i18n("No"), i18n("Save all"), i18n("Cancel"));

      // show the messagea and store result in result:

      files_close->show();

      int result=files_close->result();

      // create the save project messagebox

      // what to do
      if(result==1) // Yes- only save the actual file
      {
        // save file as if Untitled and close file
        if(isUntitled(actual_info->filename))
        {
          switchToFile(actual_info->filename);
          handledNames.append(actual_info->filename);
	        cont=fileSaveAs();
          next_info=edit_infos.first(); // start again... 'cause we deleted an entry
        }				
        else // Save file and close it
        {
          switchToFile(actual_info->filename);
          handledNames.append(actual_info->filename);
          slotFileSave();
          actual_info->modified=edit_widget->isModified();
          cont=!actual_info->modified; //something went wrong
        }
      }

      if(result==2) // No - no save but close
      {
        handledNames.append(actual_info->filename);
        actual_info->modified=false;
        removeFileFromEditlist(actual_info->filename); // immediate remove
        next_info=edit_infos.first(); // start again... 'cause we deleted an entry
      }

      if(result==3) // Save all
      {
        slotFileSaveAll();
        break;
      }

      if(result==4) // Cancel
      {
        cont=false;
	      break;
      }	
    }  // end actual file close

    actual_info=next_info;
  } // end for-loop

  // check if something went wrong with saving
  if ( cont )
  {
    for( actual_info=edit_infos.first();
         cont && actual_info != 0;
         actual_info=edit_infos.next())
    {
      if ( actual_info->modified )
        cont=false;
    } // end for-loop

    if(cont)
    {
      header_widget->clear();
      cpp_widget->clear();
      menu_buffers->clear();

      //clear all edit_infos before starting a new project
      edit_infos.clear();

      header_widget->setName(i18n("Untitled.h"));
      cpp_widget->setName(i18n("Untitled.cpp"));
      TEditInfo* edit1 = new TEditInfo;
      TEditInfo* edit2 = new TEditInfo;
      edit1->filename = header_widget->getName();
      edit2->filename = cpp_widget->getName();

      edit1->id = menu_buffers->insertItem(edit1->filename,-2,0);
      edit1->modified=false;
      edit2->id = menu_buffers->insertItem(edit2->filename,-2,0);
      edit2->modified=false;
      edit_infos.append(edit1);
      edit_infos.append(edit2);
    }
  }

  slotStatusMsg(i18n("Ready."));
}

bool CKDevelop::saveFileFromTheCurrentEditWidget(){
  QString filename=edit_widget->getName();
  TEditInfo* actual_info;
  QFileInfo file_info(filename);
  
  for(actual_info=edit_infos.first();actual_info != 0;actual_info=edit_infos.next()){
      if (actual_info->filename == filename ){
	  break;
      }
  }
  if(actual_info == 0) return false; //oops :-(

  if(file_info.lastModified() != actual_info->last_modified){
    if(QMessageBox::warning(this,i18n("File modified"),"The file " + filename +" was modified outside\n this editor.Save anyway?",QMessageBox::Yes,QMessageBox::No) == QMessageBox::No) return false;
  }
  edit_widget->doSave();
  QFileInfo file_info2(filename);
  actual_info->last_modified = file_info2.lastModified();
  return true;
}
void CKDevelop::slotFileSave(){

  QString filename=edit_widget->getName();
  QString sShownFilename=QFileInfo(filename).fileName();
  slotStatusMsg(i18n("Saving file ")+sShownFilename);
  
 

  if(isUntitled(filename)){
    slotFileSaveAs();
  }
  else{
      saveFileFromTheCurrentEditWidget(); // save the current file
      setInfoModified(filename, edit_widget->isModified());
      // only refresh if header file changed
      if(CProject::getType(filename)==CPP_HEADER){
	  slotViewRefresh();
      }
  }
  slotStatusMsg(i18n("Ready."));
  QString sHelpMsg=i18n("File ");
  sHelpMsg+=sShownFilename;
  if (edit_widget->isModified())
    sHelpMsg+=i18n(" not saved.");
  else
      sHelpMsg+=i18n(" saved.");

  slotStatusHelpMsg(sHelpMsg);
}

void CKDevelop::slotFileSaveAs(){
    slotStatusMsg(i18n("Save file as..."));
    
    fileSaveAs();

    setMainCaption();
    slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotFileSaveAll(){
    QStrList handledNames;
    // ok,its a dirty implementation  :-)
    if(!bAutosave || !saveTimer->isActive()){
	slotStatusMsg(i18n("Saving all changed files..."));
    }
    else{
	slotStatusMsg(i18n("Autosaving..."));
    }
    TEditInfo* actual_info;
    bool mod = false;
    // save current filename to switch back after saving
    QString visibleFile = edit_widget->getName();
    // ooops...autosave switches tabs...
    int visibleTab=s_tab_view->getCurrentTab();
    // first the 2 current edits
    view->setUpdatesEnabled(false);
    
    setInfoModified(header_widget->getName(), header_widget->isModified());
    setInfoModified(cpp_widget->getName(), cpp_widget->isModified());
    
    statProg->setTotalSteps(edit_infos.count());
    statProg->show();
    statProg->setProgress(0);
    int i=0;
    for(actual_info=edit_infos.first();actual_info != 0;){
	//KDEBUG1(KDEBUG_INFO,CKDEVELOP,"check file: %s",actual_info->filename.data());
	TEditInfo *next_info=edit_infos.next();
	// get now the next info... fileSaveAs can delete the actual_info
	i++;
	statProg->setProgress(i);
	if(actual_info->modified && handledNames.contains(actual_info->filename)<1){
	    if(isUntitled(actual_info->filename)){
		switchToFile(actual_info->filename);
		handledNames.append(actual_info->filename);
		if (fileSaveAs())
		    {
			// maybe saved with another name... so we have to start again
			next_info=edit_infos.first();
			i=0;
		    }
		//  KDEBUG1(KDEBUG_INFO,CKDEVELOP,"file: %s UNTITLED",actual_info->filename.data());
		//  mod = true;  this is now handled by slotFileSaveAs()
	    }
	    else{
		switchToFile(actual_info->filename,false,false);
		handledNames.append(actual_info->filename);
		saveFileFromTheCurrentEditWidget();
		actual_info->modified=edit_widget->isModified();
		// 	KDEBUG1(KDEBUG_INFO,CKDEVELOP,"file: %s ",actual_info->filename.data());
		if(actual_info->filename.right(2)==".h" || actual_info->filename.right(4)==".hxx")
		    mod = true;
	    }
	}
	actual_info=next_info;
    }
    statProg->hide();
    statProg->reset();
    if(mod){
	slotViewRefresh();
    }
    // switch back to visible file
    if (visibleTab == CPP || visibleTab == HEADER)
	{
	    // Does the visible file still exist??
	    for(actual_info=edit_infos.first();actual_info != 0 && actual_info->filename != visibleFile;
		actual_info=edit_infos.next());
	    
	    if (actual_info)
		switchToFile(visibleFile,false,false); // no force reload and no box if modified outside
	    
	    view->repaint();
	}
    view->setUpdatesEnabled(true);
    // switch back to visible tab
    s_tab_view->setCurrentTab(visibleTab);
    slotStatusMsg(i18n("Ready."));
}


void CKDevelop::slotFilePrint(){
  QString file;
  slotFileSave();
/*  if (s_tab_view->getCurrentTab() == BROWSER) {
  	file = browser_widget->currentURL();
  	CPrintDlg* printerdlg = new CPrintDlg(this, file, "suzus", TRUE);
 	printerdlg->resize(600,480);
	  printerdlg->exec();
  	delete (printerdlg);
  }
  else { */
	  file = edit_widget->getName();
	  CPrintDlg* printerdlg = new CPrintDlg(this, file, "suzus");
	  printerdlg->resize(600,480);
	  printerdlg->exec();
  	delete (printerdlg);
//	}
}

void CKDevelop::slotFileQuit(){
  slotStatusMsg(i18n("Exiting..."));
  saveOptions();
  close();
}

///////////////////////////////////////////////////////////////////////////////////////
// EDIT-Menu slots
///////////////////////////////////////////////////////////////////////////////////////

void CKDevelop::slotEditUndo(){
  edit_widget->undo();
}
void CKDevelop::slotEditRedo(){
  edit_widget->redo();
}
void CKDevelop::slotEditCut(){
  slotStatusMsg(i18n("Cutting..."));
  edit_widget->cut();
  slotStatusMsg(i18n("Ready."));
}
void CKDevelop::slotEditCopy(){
  slotStatusMsg(i18n("Copying..."));
  if(s_tab_view->getCurrentTab()==2){
    browser_widget->slotCopyText();
  }
	else
    edit_widget->copyText();
  slotStatusMsg(i18n("Ready."));
}
void CKDevelop::slotEditPaste(){
  slotStatusMsg(i18n("Pasting selection..."));
  edit_widget->paste();
  slotStatusMsg(i18n("Ready."));
}
void CKDevelop::slotEditInsertFile(){
  slotStatusMsg(i18n("Inserting file contents..."));
  edit_widget->insertFile();
  slotStatusMsg(i18n("Ready."));
}
void CKDevelop::slotEditSearch(){
  slotStatusMsg(i18n("Searching..."));
  edit_widget->search();
  slotStatusMsg(i18n("Ready."));
}
void CKDevelop::slotEditRepeatSearch(){
  slotStatusMsg(i18n("Repeating last search..."));
  if(s_tab_view->getCurrentTab()==BROWSER){
    browser_widget->findTextNext(QRegExp(doc_search_text));
  }
  else{
    edit_widget->searchAgain();
  }
  slotStatusMsg(i18n("Ready."));
}
void CKDevelop::slotEditSearchInFiles(){
  slotStatusMsg(i18n("Searching in Files..."));
  if(project){
    grep_dlg->setDirName(prj->getProjectDir());
  }
  grep_dlg->show();
  slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotEditSearchInFiles(QString search){
  int pos;
  slotStatusMsg(i18n("Searching in Files..."));

  if(project){
    grep_dlg->setDirName(prj->getProjectDir());
  }
  grep_dlg->show();

  search.replace(QRegExp("^\n"), "");
  pos=search.find("\n");
  if (pos>-1)
   search=search.left(pos);

  search=realSearchText2regExp(search, true);
  grep_dlg->slotSearchFor(search);
  slotStatusMsg(i18n("Ready."));
	}

void CKDevelop::slotEditSearchText(){
  QString text;
  if(s_tab_view->getCurrentTab()==BROWSER){
    browser_widget->getSelectedText(text);
  }
  else{
    text = edit_widget->markedText();
    if(text == ""){
      text = edit_widget->currentWord();
    }
  }
  slotEditSearchInFiles(text);
  slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotEditReplace(){
  slotStatusMsg(i18n("Replacing..."));
  edit_widget->replace();
  slotStatusMsg(i18n("Ready."));
}
void CKDevelop::slotEditIndent(){
	edit_widget->indent();
}
void CKDevelop::slotEditUnindent(){
	edit_widget->unIndent();
}

void CKDevelop::slotEditSpellcheck(){
	edit_widget->spellcheck();
}
void CKDevelop::slotEditSelectAll(){
  slotStatusMsg(i18n("Selecting all..."));
  edit_widget->selectAll();
  slotStatusMsg(i18n("Ready."));
}
void CKDevelop::slotEditInvertSelection(){
    edit_widget->invertSelection();
}
void CKDevelop::slotEditDeselectAll(){
    edit_widget->deselectAll();
}

///////////////////////////////////////////////////////////////////////////////////////
// VIEW-Menu slots
///////////////////////////////////////////////////////////////////////////////////////

void CKDevelop::slotViewGotoLine(){
  slotStatusMsg(i18n("Switching to selected line..."));
  edit_widget->gotoLine();
  slotStatusMsg(i18n("Ready."));
}
/** jump to the next error, based on the make output*/
void CKDevelop::slotViewNextError(){
  TErrorMessageInfo info = error_parser->getNext();
  if(info.filename != ""){
    messages_widget->setCursorPosition(info.makeoutputline-1,0);
    switchToFile(info.filename,info.errorline-1);
    if(!bKDevelop){
      switchToKDevelop();
    }
    slotStatusMsg(messages_widget->textLine(info.makeoutputline-1));
  }
  else{
    XBell(kapp->getDisplay(),100); // not a next found, beep
  }

  //enable/disable the menus/toolbars
  if(error_parser->hasNext()){
    enableCommand(ID_VIEW_NEXT_ERROR);
  }
  else{
    disableCommand(ID_VIEW_NEXT_ERROR);
  }
  
  if(error_parser->hasPrev()){
    enableCommand(ID_VIEW_PREVIOUS_ERROR);
  }
  else{
    disableCommand(ID_VIEW_PREVIOUS_ERROR);
  }
}
/** jump to the previews error, based on the make output*/
void CKDevelop::slotViewPreviousError(){
  TErrorMessageInfo info = error_parser->getPrev();
  if(info.filename != ""){
    messages_widget->setCursorPosition(info.makeoutputline-1,0);
    switchToFile(info.filename,info.errorline-1);
    if(!bKDevelop){
      switchToKDevelop();
    }
    slotStatusMsg(messages_widget->textLine(info.makeoutputline-1));
  }
  else{
    XBell(kapp->getDisplay(),100); // not a previous found, beep
  }
  //enable/disable the menus/toolbars
  if(error_parser->hasNext()){
    enableCommand(ID_VIEW_NEXT_ERROR);
  }
  else{
    disableCommand(ID_VIEW_NEXT_ERROR);
  }
  
  if(error_parser->hasPrev()){
    enableCommand(ID_VIEW_PREVIOUS_ERROR);
  }
  else{
    disableCommand(ID_VIEW_PREVIOUS_ERROR);
  }
}

void CKDevelop::slotViewTTreeView(){
  if(view_menu->isItemChecked(ID_VIEW_TREEVIEW)){
    view_menu->setItemChecked(ID_VIEW_TREEVIEW,false);
    kdlg_view_menu->setItemChecked(ID_VIEW_TREEVIEW,false);
		toolBar()->setButton(ID_VIEW_TREEVIEW,false);
		toolBar(ID_KDLG_TOOLBAR)->setButton(ID_VIEW_TREEVIEW,false);
    tree_view_pos=top_panner->separatorPos();
    top_panner->setSeparatorPos(0);
  }
  else{
    top_panner->setSeparatorPos(tree_view_pos);
    view_menu->setItemChecked(ID_VIEW_TREEVIEW,true);
    kdlg_view_menu->setItemChecked(ID_VIEW_TREEVIEW,true);
		toolBar()->setButton(ID_VIEW_TREEVIEW,true);
		toolBar(ID_KDLG_TOOLBAR)->setButton(ID_VIEW_TREEVIEW,true);
  }
  QRect rMainGeom= top_panner->geometry();
  top_panner->resize(rMainGeom.width()+1,rMainGeom.height());
  top_panner->resize(rMainGeom.width(),rMainGeom.height());
}

void CKDevelop::slotViewTOutputView(){
  if(view_menu->isItemChecked(ID_VIEW_OUTPUTVIEW)){
    view_menu->setItemChecked(ID_VIEW_OUTPUTVIEW,false);
    kdlg_view_menu->setItemChecked(ID_VIEW_OUTPUTVIEW,false);
		toolBar()->setButton(ID_VIEW_OUTPUTVIEW,false);
		toolBar(ID_KDLG_TOOLBAR)->setButton(ID_VIEW_OUTPUTVIEW,false);
    output_view_pos=view->separatorPos();
    view->setSeparatorPos(100);
  }
  else{
    view->setSeparatorPos(output_view_pos);
    view_menu->setItemChecked(ID_VIEW_OUTPUTVIEW,true);
    kdlg_view_menu->setItemChecked(ID_VIEW_OUTPUTVIEW,true);
		toolBar()->setButton(ID_VIEW_OUTPUTVIEW,true);
		toolBar(ID_KDLG_TOOLBAR)->setButton(ID_VIEW_OUTPUTVIEW,true);
  }
  QRect rMainGeom= view->geometry();
  view->resize(rMainGeom.width()+1,rMainGeom.height());
  view->resize(rMainGeom.width(),rMainGeom.height());
}


void CKDevelop::slotViewTStdToolbar(){
 if(view_menu->isItemChecked(ID_VIEW_TOOLBAR)){
   view_menu->setItemChecked(ID_VIEW_TOOLBAR,false);
    enableToolBar(KToolBar::Hide);
  }
  else{
    view_menu->setItemChecked(ID_VIEW_TOOLBAR,true);
    enableToolBar(KToolBar::Show);
  }

}
void CKDevelop::slotViewTBrowserToolbar(){
  if(view_menu->isItemChecked(ID_VIEW_BROWSER_TOOLBAR)){
    view_menu->setItemChecked(ID_VIEW_BROWSER_TOOLBAR,false);
    enableToolBar(KToolBar::Hide,ID_BROWSER_TOOLBAR);
  }
  else{
    view_menu->setItemChecked(ID_VIEW_BROWSER_TOOLBAR,true);
    enableToolBar(KToolBar::Show,ID_BROWSER_TOOLBAR);
  }
}

void CKDevelop::slotViewTStatusbar(){
  if(view_menu->isItemChecked(ID_VIEW_STATUSBAR)){
    view_menu->setItemChecked(ID_VIEW_STATUSBAR,false);
	  kdlg_view_menu->setItemChecked(ID_VIEW_STATUSBAR,false);
  }
  else{
    view_menu->setItemChecked(ID_VIEW_STATUSBAR,true);
  	kdlg_view_menu->setItemChecked(ID_VIEW_STATUSBAR,true);
  }
  enableStatusBar();
}

void CKDevelop::slotViewRefresh(){
  refreshTrees();
}



///////////////////////////////////////////////////////////////////////////////////////
// BUILD-Menu slots
///////////////////////////////////////////////////////////////////////////////////////

void CKDevelop::slotBuildCompileFile(){
  if(!CToolClass::searchProgram(make_cmd)){
    return;
  }
  error_parser->reset();
  error_parser->toogleOn();
  showOutputView(true);
  slotFileSave();
  setToolMenuProcess(false);
  slotStatusMsg(i18n("Compiling ")+cpp_widget->getName());
  messages_widget->clear();
  process.clearArguments();
  QFileInfo fileinfo(cpp_widget->getName());
  QDir::setCurrent(fileinfo.dirPath());
  // get the filename of the implementation file to compile and change extension for make
  //KDEBUG1(KDEBUG_INFO,CKDEVELOP,"ObjectFile= %s",QString(fileinfo.baseName()+".o").data());
//  cerr << "ObjectFile= " << fileinfo.baseName()+".o";
  process << make_cmd << fileinfo.baseName()+".o";
  process.start(KProcess::NotifyOnExit,KProcess::AllOutput);
}

void CKDevelop::slotBuildRun(){
  slotBuildMake();
  slotStatusMsg(i18n("Running ")+prj->getBinPROGRAM());
  beep=false;
  next_job = "run";
}

void CKDevelop::slotBuildRunWithArgs(){
    QString args=prj->getExecuteArgs();
    CExecuteArgDlg argdlg(this,"Arguments",i18n("Execute with Arguments"),args);
    if(argdlg.exec()){
	prj->setExecuteArgs(argdlg.getArguments());		
	prj->writeProject();
	slotBuildMake();
	slotStatusMsg(i18n("Running ")+prj->getBinPROGRAM());
	beep=false;
	next_job = "run_with_args";
    }
}
void CKDevelop::slotBuildDebug(){

  if(!CToolClass::searchProgram("kdbg")){
    return;
  }
  if(!prj->getBinPROGRAM()){
    slotBuildMake();
  }
  if(!bKDevelop)
    switchToKDevelop();

  showOutputView(false);
  showTreeView(false);
  
  slotStatusMsg(i18n("Running  ")+prj->getBinPROGRAM()+i18n("  in KDbg"));

  s_tab_view->setCurrentTab(TOOLS);
  swallow_widget->sWClose(false);
  QDir::setCurrent(prj->getProjectDir() + prj->getSubDir()); 
  swallow_widget->setExeString("kdbg "+ prj->getBinPROGRAM());
  swallow_widget->sWExecute();
  swallow_widget->init();
  
}

void CKDevelop::slotBuildMake(){
  if(!CToolClass::searchProgram(make_cmd)){
    return;
  }

  //save/generate dialog if needed
  kdlgedit->generateSourcecodeIfNeeded();
  error_parser->reset();
  error_parser->toogleOn();
  showOutputView(true);
  setToolMenuProcess(false);
  slotFileSaveAll();
  slotStatusMsg(i18n("Running make..."));
  messages_widget->clear();
  QDir::setCurrent(prj->getProjectDir() + prj->getSubDir()); 
  process.clearArguments();
  if(!prj->getMakeOptions().isEmpty()){
    process << make_cmd << prj->getMakeOptions();
  }
  else{
    process << make_cmd;
  }
  beep = true;
  
  process.start(KProcess::NotifyOnExit,KProcess::AllOutput);
}

// void CKDevelop::slotBuildMakeWith(){
//   KLineEditDlg *box = new KLineEditDlg(i18n("Make with :"), make_with_cmd.data(), this, true);
//   box->show();

//   if (!box->result())   /* cancelled */
//     return;

//   make_with_cmd = box->text();
//   delete box;

//   showOutputView(true);
//   setToolMenuProcess(false);
//   slotFileSaveAll();
//   slotStatusMsg(i18n("Running make..."));
//   messages_widget->clear();

//   if ( prj->getProjectType() == "normal_empty" ||
//        prj->getProjectType() == "normal_java")
//     QDir::setCurrent(prj->getProjectDir()); 
//   else
//     QDir::setCurrent(prj->getProjectDir() + prj->getSubDir()); 

//   process.clearArguments();
//   process << make_with_cmd;

//   beep = true;
//   process.start(KProcess::NotifyOnExit,KProcess::AllOutput);
// }

void CKDevelop::slotBuildRebuildAll(){
  if(!CToolClass::searchProgram(make_cmd)){
    return;
  }
  error_parser->reset();
  error_parser->toogleOn();
  showOutputView(true);

  setToolMenuProcess(false);
  slotFileSaveAll();
  slotStatusMsg(i18n("Running make clean-command "));
  messages_widget->clear();
  QDir::setCurrent(prj->getProjectDir() + prj->getSubDir()); 
  process.clearArguments();
  process << make_cmd;
  process << "clean";
  next_job = make_cmd; // checked in slotProcessExited()
  beep = true;
  process.start(KProcess::NotifyOnExit,KProcess::AllOutput);
}
void CKDevelop::slotBuildCleanRebuildAll(){
  if(!CToolClass::searchProgram(make_cmd)){
    return;
  }

  QString flaglabel=(prj->getProjectType()=="normal_c") ? "CFLAGS=\"" : "CXXFLAGS=\"";

  error_parser->reset();
  error_parser->toogleOn();
  showOutputView(true);
  setToolMenuProcess(false);
  slotFileSaveAll();
  messages_widget->clear();
  slotStatusMsg(i18n("Running make clean and rebuilding all..."));
  QDir::setCurrent(prj->getProjectDir()); 
  shell_process.clearArguments();
  shell_process << make_cmd << "distclean && " << make_cmd 
		<< " -f Makefile.dist && ";
  shell_process << flaglabel;
  if (!prj->getCXXFLAGS().isEmpty() || !prj->getAdditCXXFLAGS().isEmpty())
  {
       if (!prj->getCXXFLAGS().isEmpty())
          shell_process << prj->getCXXFLAGS() << " ";
       if (!prj->getAdditCXXFLAGS().isEmpty())
          shell_process << prj->getAdditCXXFLAGS();
  }
  shell_process  << "\" " << "LDFLAGS=\" " ;
  if (!prj->getLDFLAGS().isEmpty())
         shell_process << prj->getLDFLAGS();
  shell_process  << "\" "<< "./configure && " << make_cmd;

  beep = true;
  shell_process.start(KProcess::NotifyOnExit,KProcess::AllOutput);
}

void CKDevelop::slotBuildDistClean(){
  if(!CToolClass::searchProgram(make_cmd)){
    return;
  }
  error_parser->reset();
  error_parser->toogleOn();
  showOutputView(true);
  setToolMenuProcess(false);
  slotFileSaveAll();
  slotStatusMsg(i18n("Running make distclean..."));
  messages_widget->clear();
  QDir::setCurrent(prj->getProjectDir());
  process.clearArguments();
  process << make_cmd << "distclean";
  process.start(KProcess::NotifyOnExit,KProcess::AllOutput);
}
void CKDevelop::slotBuildAutoconf(){
  if(!CToolClass::searchProgram("automake")){
    return;
  }
  if(!CToolClass::searchProgram("autoconf")){
    return;
  }
  showOutputView(true);
  error_parser->toogleOff();
  setToolMenuProcess(false);
  slotFileSaveAll();
  slotStatusMsg(i18n("Running autoconf/automake suite..."));
  messages_widget->clear();
  QDir::setCurrent(prj->getProjectDir());
  shell_process.clearArguments();
  if(QFileInfo(QDir::current(),"Makefile.dist").exists())
    shell_process << make_cmd << " -f Makefile.dist";
  else
    shell_process << make_cmd << " -f Makefile.cvs";

  shell_process.start(KProcess::NotifyOnExit,KProcess::AllOutput);
  beep = true;
}


void CKDevelop::slotBuildConfigure(){

    QString args=prj->getConfigureArgs();
    CExecuteArgDlg argdlg(this,"Arguments",i18n("Configure with Arguments"),args);
    if(argdlg.exec()){
	prj->setConfigureArgs(argdlg.getArguments());		
	prj->writeProject();
    }
    else{
	return;
    }

  slotStatusMsg(i18n("Running ./configure..."));
  QString flaglabel=(prj->getProjectType()=="normal_c") ? "CFLAGS=\"" : "CXXFLAGS=\"";

  showOutputView(true);
  setToolMenuProcess(false);
  error_parser->toogleOff();
  slotFileSave();
  messages_widget->clear();
  slotFileSaveAll();
  QDir::setCurrent(prj->getProjectDir()); 
  shell_process.clearArguments();
  shell_process << flaglabel;
  if (!prj->getCXXFLAGS().isEmpty() || !prj->getAdditCXXFLAGS().isEmpty())
  {
      if (!prj->getCXXFLAGS().isEmpty())
          shell_process << prj->getCXXFLAGS() << " ";
      if (!prj->getAdditCXXFLAGS().isEmpty())
          shell_process << prj->getAdditCXXFLAGS();
  }
  shell_process  << "\" " << "LDFLAGS=\" " ;
  if (!prj->getLDFLAGS().isEmpty())
         shell_process << prj->getLDFLAGS();
  shell_process  << "\" "<< "./configure " << argdlg.getArguments();
  shell_process.start(KProcess::NotifyOnExit,KProcess::AllOutput);
  beep = true;
}


void CKDevelop::slotBuildStop(){
  slotStatusMsg(i18n("Killing current process..."));
  setToolMenuProcess(true);
  process.kill();
  shell_process.kill();
  appl_process.kill();
  slotStatusMsg(i18n("Ready."));
}


///////////////////////////////////////////////////////////////////////////////////////
// TOOLS-Menu slots
///////////////////////////////////////////////////////////////////////////////////////


// void CKDevelop::slotToolsKDbg(){

//   if(!CToolClass::searchProgram("kdbg")){
//     return;
//   }
//   if(!bKDevelop)
//     switchToKDevelop();

//   showOutputView(false);

//   s_tab_view->setCurrentTab(TOOLS);
//   swallow_widget->sWClose(false);
//   swallow_widget->setExeString("kdbg");
//   swallow_widget->sWExecute();
//   swallow_widget->init();
// }

void CKDevelop::slotToolsTool(int tool){
/*    switch(tool){
    case ID_TOOLS_KDLGEDIT:
	return;
	break;
    case ID_KDLG_TOOLS_KDEVELOP:
	return;
	break;
    }*/
    if(!CToolClass::searchProgram(tools_exe.at(tool)) ){
	return;
    }
    if(!bKDevelop)
	switchToKDevelop();
    
    showOutputView(false);

    QString argument=tools_argument.at(tool);
 		
		// This allows us to replace the macro %H with the header file name, %S with the source file name
		// and %D with the project directory name.  Any others we should have?
    argument.replace( QRegExp("%H"), header_widget->getName() );
		argument.replace( QRegExp("%S"), cpp_widget->getName() );
    argument.replace( QRegExp("%D"), prj->getProjectDir() );
		s_tab_view->setCurrentTab(TOOLS);
    swallow_widget->sWClose(false);
    if(argument.isEmpty()){
  	swallow_widget->setExeString(tools_exe.at(tool));
    }
    else{
  	swallow_widget->setExeString(tools_exe.at(tool)+argument);
    }
    swallow_widget->sWExecute();
    swallow_widget->init();
}



///////////////////////////////////////////////////////////////////////////////////////
// OPTIONS-Menu slots
///////////////////////////////////////////////////////////////////////////////////////

void CKDevelop::slotOptionsEditor(){
  slotStatusMsg(i18n("Setting up the Editor..."));
  cpp_widget->optDlg();
  config->setGroup("KWrite Options");
  cpp_widget->writeConfig(config);
  cpp_widget->doc()->writeConfig(config);
  header_widget->copySettings(cpp_widget);
  config->setGroup("KWrite Options");
  header_widget->readConfig(config);
  header_widget->doc()->readConfig(config);
  slotStatusMsg(i18n("Ready."));

}
void CKDevelop::slotOptionsEditorColors(){
  slotStatusMsg(i18n("Setting up the Editor's colors..."));
  cpp_widget->colDlg();
  config->setGroup("KWrite Options");
  cpp_widget->writeConfig(config);
  cpp_widget->doc()->writeConfig(config);
  header_widget->copySettings(cpp_widget);
  config->setGroup("KWrite Options");
  header_widget->readConfig(config);
  header_widget->doc()->readConfig(config);
  slotStatusMsg(i18n("Ready."));

}


void CKDevelop::slotOptionsSyntaxHighlightingDefaults(){
  slotStatusMsg(i18n("Setting up syntax highlighting default colors..."));
  cpp_widget->hlDef();
  config->setGroup("KWrite Options");
  cpp_widget->writeConfig(config);
  cpp_widget->doc()->writeConfig(config);
  header_widget->copySettings(cpp_widget);
  config->setGroup("KWrite Options");
  header_widget->readConfig(config);
  header_widget->doc()->readConfig(config);
  slotStatusMsg(i18n("Ready."));
}
void CKDevelop::slotOptionsSyntaxHighlighting(){
  slotStatusMsg(i18n("Setting up syntax highlighting colors..."));
  cpp_widget->hlDlg();
  config->setGroup("KWrite Options");
  cpp_widget->writeConfig(config);
  cpp_widget->doc()->writeConfig(config);
  header_widget->copySettings(cpp_widget);
  config->setGroup("KWrite Options");
  header_widget->readConfig(config);
  header_widget->doc()->readConfig(config);
  slotStatusMsg(i18n("Ready."));
}
void CKDevelop::slotOptionsDocBrowser(){
   slotStatusMsg(i18n("Configuring Documentation Browser..."));

   CDocBrowserOptionsDlg browserOptions;

   connect( browserOptions.fontOptions, SIGNAL(fontSize(int)),
		 browser_widget, SLOT(slotDocFontSize( int )) );
   connect( browserOptions.fontOptions, SIGNAL(standardFont( const char * )),
		 browser_widget, SLOT(slotDocStandardFont( const char * )) );
   connect( browserOptions.fontOptions, SIGNAL(fixedFont( const char * )),
		 browser_widget, SLOT(slotDocFixedFont( const char * )) );
   connect( browserOptions.colorOptions, SIGNAL(colorsChanged(const QColor&, const QColor&,
			const QColor&, const QColor&, const bool, const bool)),
		 browser_widget, SLOT(slotDocColorsChanged(const QColor&, const QColor&,
            		const QColor&, const QColor&, const bool, const bool)) );

   browserOptions.show();
   slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotOptionsToolsConfigDlg(){
  slotStatusMsg(i18n("Configuring Tools-Menu entries..."));
  CToolsConfigDlg* configdlg= new CToolsConfigDlg(this,"configdlg");
  configdlg->show();

  tools_menu->clear();
  kdlg_tools_menu->clear();
	setToolmenuEntries();
  slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotOptionsSpellchecker(){

}

void CKDevelop::slotOptionsConfigureEnscript(){
  if (!CToolClass::searchProgram("enscript")) {
    return;
  }
  enscriptconf = new CConfigEnscriptDlg(this, "confdialog");
  enscriptconf->resize(610,510);
  enscriptconf->exec();
  delete (enscriptconf);
}

void CKDevelop::slotOptionsConfigureA2ps(){
  if (!CToolClass::searchProgram("a2ps")) {
    return;
  }
  a2psconf = new CConfigA2psDlg(this, "confdialog");
  a2psconf->resize(600,430);
  a2psconf->exec();
  delete (a2psconf);
}

void CKDevelop::slotOptionsKDevelop(){
  slotStatusMsg(i18n("Setting up KDevelop..."));

  CKDevSetupDlg* setup= new CKDevSetupDlg(this,"Setup",accel);
  setup->show();
	accel->readSettings();
	setKeyAccel();
  slotStatusMsg(i18n("Ready."));
}
// slots needed by the KDevelop Setup
void CKDevelop::slotOptionsMake(){
  config->setGroup("General Options");
  make_cmd=config->readEntry("Make","make");

}

void CKDevelop::slotOptionsAutosave(bool autosave){

  bAutosave=autosave;
  if(bAutosave)
    saveTimer->start(saveTimeout);
  else
    saveTimer->stop();
}

void CKDevelop::slotOptionsAutosaveTime(int time){

  switch(time){
  case 0:
    saveTimeout=3*60*1000;
    break;
  case 1:
    saveTimeout=5*60*1000;
    break;
  case 2:
    saveTimeout=15*60*1000;
    break;
  case 3:
    saveTimeout=30*60*1000;
    break;
  }
  saveTimer->changeInterval(saveTimeout);
}

void CKDevelop::slotOptionsAutoswitch(bool autoswitch){
  bAutoswitch=autoswitch;
}

void CKDevelop::slotOptionsDefaultCV(bool defaultcv){
  bDefaultCV=defaultcv;
}

void CKDevelop::slotOptionsUpdateKDEDocumentation(){
  if(!CToolClass::searchProgram("kdoc")){
    return;
  }
  slotStatusMsg(i18n("Updating KDE-Libs documentation..."));
  config->setGroup("Doc_Location");
  CUpdateKDEDocDlg dlg(this,"test",&shell_process, config);
  if(dlg.exec()){
    slotStatusMsg(i18n("Generating Documentation..."));
    setToolMenuProcess(false);
  }
}
void CKDevelop::slotOptionsCreateSearchDatabase(){
  if(!CToolClass::searchProgram("glimpseindex")){
    return;
  }
  CCreateDocDatabaseDlg dlg(this,"DLG",&shell_process,config);
  if(dlg.exec()){
    slotStatusMsg(i18n("Creating Search Database..."));
  }

  return;

}
	
void CKDevelop::slotBookmarksSet(){
	if(s_tab_view->getCurrentTab()==BROWSER)
		slotBookmarksAdd();
	else{
		if(edit_widget==header_widget)
			header_widget->setBookmark();
		if(edit_widget==cpp_widget)
			cpp_widget->setBookmark();
	}
}
void CKDevelop::slotBookmarksAdd(){
	if(s_tab_view->getCurrentTab()==BROWSER){
		doc_bookmarks->clear();
		doc_bookmarks_list.append(browser_widget->currentURL());
		doc_bookmarks_title_list.append(browser_widget->currentTitle());
		
		uint i;
    for ( i =0 ; i < doc_bookmarks_list.count(); i++){
      doc_bookmarks->insertItem(Icon("mini/html.xpm"),doc_bookmarks_title_list.at(i));
    }
	}
	if(edit_widget==header_widget)
		header_widget->addBookmark();
	if(edit_widget==cpp_widget)
		cpp_widget->addBookmark();

}
void CKDevelop::slotBookmarksClear(){
	if(s_tab_view->getCurrentTab()==BROWSER){
		doc_bookmarks_list.clear();
		doc_bookmarks_title_list.clear();
		doc_bookmarks->clear();
	}		
	else{
		if(edit_widget==header_widget)
			header_widget->clearBookmarks();
		if(edit_widget==cpp_widget)
			cpp_widget->clearBookmarks();
	}	
}

void CKDevelop::slotBoomarksBrowserSelected(int id_){
	slotStatusMsg(i18n("Opening bookmark..."));
	QString file= doc_bookmarks_list.at(id_);
	slotURLSelected( browser_widget, file,1,"test");	
  slotStatusMsg(i18n("Ready."));
}	


///////////////////////////////////////////////////////////////////////////////////////
// HELP-Menu slots
///////////////////////////////////////////////////////////////////////////////////////
void CKDevelop::slotHelpBack(){
  slotStatusMsg(i18n("Switching to last page..."));
  QString str = history_list.prev();
  if (str != 0){
    if(!bKDevelop)
      switchToKDevelop();
    s_tab_view->setCurrentTab(BROWSER);
    browser_widget->showURL(str);
  }

  //KDEBUG1(KDEBUG_INFO,CKDEVELOP,"COUNT HISTORYLIST: %d",history_list.count());
  slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotHelpForward(){
  slotStatusMsg(i18n("Switching to next page..."));
  QString str = history_list.next();
  if (str != 0){
    if(!bKDevelop)
      switchToKDevelop();
    s_tab_view->setCurrentTab(BROWSER);
    browser_widget->showURL(str);
  }

  slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotHelpHistoryBack( int id_){
	slotStatusMsg(i18n("Opening history page..."));
	
  QString str = history_list.at(id_);
  if (str != 0){
    if(!bKDevelop)
      switchToKDevelop();
    s_tab_view->setCurrentTab(BROWSER);
    browser_widget->showURL(str);
  }

  slotStatusMsg(i18n("Ready."));

}

void CKDevelop::slotHelpHistoryForward( int id_){
	slotStatusMsg(i18n("Opening history page..."));

  int cur=history_list.at()+1;	
  QString str = history_list.at(cur+id_);
  if (str != 0){
    if(!bKDevelop)
      switchToKDevelop();
    s_tab_view->setCurrentTab(BROWSER);
    browser_widget->showURL(str);
  }

  slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotHelpBrowserReload(){
	slotStatusMsg(i18n("Reloading page..."));
  if(!bKDevelop)
    switchToKDevelop();
  s_tab_view->setCurrentTab(BROWSER);
  browser_widget->setFocus();
	browser_widget->showURL(browser_widget->currentURL(), true);
	slotStatusMsg(i18n("Ready."));
}

void CKDevelop::slotHelpSearchText(QString text){
  int pos;

  if(!CToolClass::searchProgram("glimpse")){
    return;
  }

  /// stripping error causing \n's
  if(!text.isEmpty())
  {
   text.replace(QRegExp("^\n"), "");
   pos=text.find("\n");
   if (pos>-1)
    text=text.left(pos);

   text.replace(QRegExp("'"), "'\\''"); // handle ' in a right way
  }

  if(text.isEmpty()){
    KMsgBox::message(this,i18n("Error..."),i18n("You must select a text for searching the documentation!"));
    return;
  }
  //  cerr << ":" << text << ":" << endl;

  doc_search_display_text = text.copy(); // save the text
  text=realSearchText2regExp(text);  // change the text for using with regexp
  doc_search_text = text.copy();

  slotStatusMsg(i18n("Searching selected text in documentation..."));
  if(!QFile::exists(KApplication::localkdedir()+"/share/apps" + "/kdevelop/.glimpse_index")){
    if(KMsgBox::yesNo(this,i18n("Error..."),i18n("KDevelop couldn't find the search database.\n Do you want to generate it now?")) == 1){
      slotOptionsCreateSearchDatabase();
    }
    return;
  }
	enableCommand(ID_HELP_BROWSER_STOP);
  search_output = ""; // delete all from the last search
  search_process.clearArguments();
  search_process << "glimpse  -H "+ KApplication::localkdedir()+"/share/apps" + "/kdevelop -U -c -y '"+ text +"'";
  search_process.start(KShellProcess::NotifyOnExit,KShellProcess::AllOutput);
}
void CKDevelop::slotHelpSearchText(){
  QString text;
  if(s_tab_view->getCurrentTab()==BROWSER){
    browser_widget->getSelectedText(text);
  }
  else{
    text = edit_widget->markedText();
    if(text == ""){
      text = edit_widget->currentWord();
    }
  }

  slotHelpSearchText(text);
}

void CKDevelop::slotHelpSearch(){
  slotStatusMsg(i18n("Searching for Help on..."));
  CFindDocTextDlg* help_srch_dlg=new CFindDocTextDlg(this,"Search_for_Help_on");
  connect(help_srch_dlg,SIGNAL(signalFind(QString)),this,SLOT(slotHelpSearchText(QString)));
  help_srch_dlg->exec();
	delete help_srch_dlg;
}

void CKDevelop::slotHelpReference(){

  QString strpath = KApplication::kde_htmldir().copy() + "/";
  QString file;
  // first try the locale setting
  file = strpath + klocale->language() + '/' + "kdevelop/reference/C/cref.html";
  if( !QFileInfo( file ).exists() ){
  // not found: use the default
  	file = strpath + "default/" + "kdevelop/reference/C/cref.html";
  }
  if( !QFileInfo( file ).exists() ){
  file = strpath + klocale->language() + '/' + "kdevelop/cref.html";
  }
  if( !QFileInfo( file ).exists() ){
    // not found: use the default
    file = strpath + "default/" + "kdevelop/cref.html";
  }
  slotURLSelected(browser_widget,"file:" + file,1,"test");
}


void CKDevelop::slotHelpQtLib(){
  config->setGroup("Doc_Location");
  QString doc_qt = config->readEntry("doc_qt", QT_DOCDIR);
  slotURLSelected(browser_widget,"file:" + doc_qt + "/index.html",1,"test");
}


void CKDevelop::showLibsDoc(const char *libname)
{
  config->setGroup("Doc_Location");
  QString doc_kde = config->readEntry("doc_kde", KDELIBS_DOCDIR);
  QString url = "file:" + doc_kde + libname + "/index.html";
  slotURLSelected(browser_widget, url,1,"test");
}
    
void CKDevelop::slotHelpKDECoreLib(){
  showLibsDoc("kdecore");
}


void CKDevelop::slotHelpKDEGUILib(){
  showLibsDoc("kdeui");
}


void CKDevelop::slotHelpKDEKFileLib(){
  showLibsDoc("kfile");
}


void CKDevelop::slotHelpKDEHTMLLib(){
  config->setGroup("Doc_Location");
  QString doc_kde = config->readEntry("doc_kde", KDELIBS_DOCDIR);
  QString file = doc_kde + "khtmlw/index.html";
  if( !QFileInfo( file ).exists() ){
    // not found: use khtml
    file = doc_kde + "khtml/index.html";
  }
  slotURLSelected(browser_widget,"file:" +file  ,1,"test");
}


void CKDevelop::slotHelpAPI(){
  if(project){
    QString api_file=prj->getProjectDir() + prj->getSubDir() +  "api/index.html";
    if(!QFileInfo(api_file).exists()){
    	int result=KMsgBox::yesNo( this, i18n("No Project API documentation !"), i18n("The Project API documentation is not present.\n" 
    																																	"Would you like to generate it now ?"), KMsgBox::QUESTION);
    	if(result==1){
    		slotProjectAPI();
			}
			else{
				return;
			}
		}
		else{
	    slotStatusMsg(i18n("Switching to project API Documentation..."));
	    slotURLSelected(browser_widget,api_file,1,"test");     
			slotStatusMsg(i18n("Ready.")); 
		}
  }
}
void CKDevelop::slotHelpManual(){
  if(project){

    QString name = prj->getSGMLFile().copy();
    QFileInfo finfo(name);
 
    QString doc_file = finfo.dirPath() +"/"+finfo.baseName()+ ".html";
    if(!QFileInfo(doc_file).exists()){
    	int result=KMsgBox::yesNo( this, i18n("No Project manual found !"),
    				i18n("The Project manual documentation is not present.\n" 
    							"Would you like to generate the handbook  now ?"), KMsgBox::QUESTION);
    	if(result==1){
    		slotProjectManual();
			}
			else{
				return;
			}
		}
		else{
	    slotStatusMsg(i18n("Switching to project Manual..."));
  	  slotURLSelected(browser_widget,doc_file,1,"test");
    	slotStatusMsg(i18n("Ready."));
  	}
  }
}

void CKDevelop::slotHelpContents(){
    
  QString strpath = KApplication::kde_htmldir().copy() + "/";
  QString file;
  // first try the locale setting
  file = strpath + klocale->language() + '/' + "kdevelop/index.html";
  
  if( !QFileInfo( file ).exists() ){
    // not found: use the default
    file = strpath + "default/" + "kdevelop/index.html";
  }
  slotURLSelected(browser_widget,"file:" + file,1,"test");
}

void CKDevelop::slotHelpTutorial(){

  QString strpath = KApplication::kde_htmldir().copy() + "/";
  QString file;
  // first try the locale setting
  file = strpath + klocale->language() + '/' + "kdevelop/programming/index.html";

  if( !QFileInfo( file ).exists() ){
    // not found: use the default
    file = strpath + "default/" + "kdevelop/programming/index.html";
  }
  slotURLSelected(browser_widget,"file:" + file,1,"test");
	
}
void CKDevelop::slotHelpTipOfDay(){
	KTipofDay* tipdlg=new KTipofDay(this, "tip of the day");
	tipdlg->show();

	delete tipdlg;	
}




void CKDevelop::slotHelpHomepage(){
  if(vfork() > 0) {
    // drop setuid, setgid
    setgid(getgid());
    setuid(getuid());
    
    execlp("kfmclient", "kfmclient", "exec", QString("http://www.kdevelop.org").data(), 0);
    _exit(0);
  }
}
void CKDevelop::slotHelpBugReport(){
    
    config->setGroup("General Options");
    TBugReportInfo info;
    info.author = config->readEntry("author_name","");
    info.email = config->readEntry("author_email","");
    info.os = config->readEntry("os","");
    info.kde_version = config->readEntry("kde_version","");
    info.qt_version = config->readEntry("qt_version","");
    info.compiler = config->readEntry("compiler","");

    CBugReportDlg dlg(this,"bug",info,config->readEntry("kdevelop_bug_report_email","submit@bugs.kde.org"));
    if( dlg.exec()){
	config->writeEntry("author_name",dlg.name);
	config->writeEntry("author_email",dlg.email_address);
	config->writeEntry("os",dlg.os);
	config->writeEntry("qt_version",dlg.qt_version);
	config->writeEntry("kde_version",dlg.kde_version);
	config->writeEntry("compiler",dlg.compiler);
    }
    
    
}
void CKDevelop::slotHelpAbout(){
  QMessageBox aboutmsg(this, "About KDevelop");
  aboutmsg.setCaption(i18n("About KDevelop..."));
  aboutmsg.setButtonText(1, i18n("OK"));
  aboutmsg.setText(i18n("\t   KDevelop Version "+version+" \n\n"
  											"\t(c) 1998,1999 The KDevelop Team \n\n"
                        "Sandy Meier <smeier@rz.uni-potsdam.de>\n"
                        "Stefan Heidrich <sheidric@rz.uni-potsdam.de>\n"
                        "Ralf Nolden <Ralf.Nolden@post.rwth-aachen.de>\n"
                        "Jonas Nordin <jonas.nordin@cenacle.se>\n"
                        "Pascal Krahmer <pascal@beast.de>\n"
			"Bernd Gehrmann <bernd@physik.hu-berlin.de>\n"
                        "Stefan Bartel <bartel@rz.uni-potsdam.de>\n"
			"J�rgen Olsson<jorgen@cenacle.net>\n"
                        "Martin Piskernig <martin.piskernig@stuwo.at>\n\n"
                        "See The KDevelop User Manual, Chapter Authors\n"
                        "for further information.\n\n"
                        "This program is free software; you can\n"
                        "redistribute it and/or modify it under\n"
                        "the terms of the GNU General Public License\n"
                        "as published by the Free Software Foundation;\n"
                        "either version 2 of the License, or (at your\n"
                        "option) any later version.\n\n"
												"This program is distributed in the hope that\n"
												"it will be useful, but WITHOUT ANY WARRANTY; \n"
												"without even the implied warranty of\n"
												"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
												"See the GNU General Public License for more details."));

  QPixmap pm;
  pm.load(KApplication::kde_datadir() + "/kdevelop/pics/about_logo.bmp");
  aboutmsg.setIconPixmap(pm);
  aboutmsg.show();
}

void CKDevelop::slotKDlgViewPropView(){
  if(kdlg_view_menu->isItemChecked(ID_KDLG_VIEW_PROPVIEW)){
    kdlg_view_menu->setItemChecked(ID_KDLG_VIEW_PROPVIEW,false);
    properties_view_pos=kdlg_top_panner->separatorPos();
    kdlg_top_panner->setSeparatorPos(100);
  }
  else{
    kdlg_top_panner->setSeparatorPos(properties_view_pos);
    kdlg_view_menu->setItemChecked(ID_KDLG_VIEW_PROPVIEW,true);
  }
  QRect rMainGeom= kdlg_top_panner->geometry();
  kdlg_top_panner->resize(rMainGeom.width()+1,rMainGeom.height());
  kdlg_top_panner->resize(rMainGeom.width(),rMainGeom.height());

}
void CKDevelop::slotKDlgViewToolbar(){
  if(kdlg_view_menu->isItemChecked(ID_KDLG_VIEW_TOOLBAR)){
    kdlg_view_menu->setItemChecked(ID_KDLG_VIEW_TOOLBAR,false);
    enableToolBar(KToolBar::Hide,ID_KDLG_TOOLBAR);
  }
  else{
    kdlg_view_menu->setItemChecked(ID_KDLG_VIEW_TOOLBAR,true);
    enableToolBar(KToolBar::Show,ID_KDLG_TOOLBAR);
  }
}

void CKDevelop::slotKDlgViewStatusbar(){

}

/*********************************************************************
 *                                                                   *
 *             SLOTS FOR THE CLASSBROWSER WIZARD BUTTON              *
 *                                                                   *
 ********************************************************************/

/*---------------------------- CKDevelop::slotClassbrowserViewClass()
 * slotClassbrowserViewClass()
 *   Event when the user wants to view the classdeclaration from the
 *   browser toolbar.
 *
 * Parameters:
 *   -
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotClassbrowserViewClass()
{
  KCombo* classCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_CLASS_CHOICE);
  QString classname = classCombo->currentText();

  CVGotoDeclaration( classname, "", THCLASS );
}

/*---------------------------- CKDevelop::slotClassbrowserViewDeclaration()
 * slotClassbrowserViewDeclaration()
 *   Event when the user wants to view a declaration from the
 *   browser toolbar/menu.
 *
 * Parameters:
 *   -
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotClassbrowserViewDeclaration()
{
  KCombo* classCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_CLASS_CHOICE);
  KCombo* methodCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_METHOD_CHOICE);
  QString classname = classCombo->currentText();
  QString methodname = methodCombo->currentText();

  CVGotoDeclaration( classname, methodname, 
                     ( methodname.isEmpty() ? THCLASS : THPUBLIC_METHOD ) );
}

/*----------------------- CKDevelop::slotClassbrowserViewDefinition()
 * slotClassbrowserViewDefinition()
 *   Event when the user wants to view a definition from the  browser
 *   toolbar/menu.
 *
 * Parameters:
 *   -
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotClassbrowserViewDefinition()
{
  KCombo* classCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_CLASS_CHOICE);
  KCombo* methodCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_METHOD_CHOICE);
  QString classname = classCombo->currentText();
  QString methodname = methodCombo->currentText();

  if( methodname.isEmpty() )
    CVGotoDeclaration( classname, "", THCLASS );
  else
    CVGotoDefinition( classname, methodname, THPUBLIC_METHOD );
}

/*------------------------------ CKDevelop::slotClassbrowserNewMethod()
 * slotClassbrowserNewMethod()
 *   Event when the user wants to create a new method from the browser
 *   toolbar/menu.
 *
 * Parameters:
 *   -
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotClassbrowserNewMethod()
{
  KCombo* classCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_CLASS_CHOICE);
  QString classname = classCombo->currentText();

  if( !classname.isEmpty() )
    slotCVAddMethod( classname );
}

/*------------------------- CKDevelop::slotClassbrowserNewAttribute()
 * slotClassbrowserNewAttribute()
 *   Event when the user wants to create a new attribute from the 
 *   browser toolbar/menu.
 *
 * Parameters:
 *   -
 * Returns:
 *   -
 *-----------------------------------------------------------------*/
void CKDevelop::slotClassbrowserNewAttribute()
{
  KCombo* classCombo = toolBar(ID_BROWSER_TOOLBAR)->getCombo(ID_CV_TOOLBAR_CLASS_CHOICE);
  QString classname = classCombo->currentText();

  if( !classname.isEmpty() )
    slotCVAddAttribute( classname );
}

/////////////////////////////////////////////////////////////////////
// Other slots and functions needed
/////////////////////////////////////////////////////////////////////

void CKDevelop::slotStatusMsg(const char *text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
//  statusBar()->clear();
    statProg->hide();
    statusBar()->changeItem(text, ID_STATUS_MSG );
}


void CKDevelop::slotStatusHelpMsg(const char *text)
{
  ///////////////////////////////////////////////////////////////////
    // change status message of whole statusbar temporary (text, msec)
    statusBar()->message(text, 2000);
    QTimer::singleShot ( 2000, statProg, SLOT(hide()) );

}

void CKDevelop::enableCommand(int id_)
{
  kdev_menubar->setItemEnabled(id_,true);
  kdlg_menubar->setItemEnabled(id_,true);
//  menuBar()->setItemEnabled(id_,true);
  toolBar()->setItemEnabled(id_,true);
  toolBar(ID_BROWSER_TOOLBAR)->setItemEnabled(id_,true);
  toolBar(ID_KDLG_TOOLBAR)->setItemEnabled(id_,true);
}

void CKDevelop::disableCommand(int id_)
{
  kdev_menubar->setItemEnabled(id_,false);
  kdlg_menubar->setItemEnabled(id_,false);
//  menuBar()->setItemEnabled(id_,false);
  toolBar()->setItemEnabled(id_,false);
  toolBar(ID_BROWSER_TOOLBAR)->setItemEnabled(id_,false);
  toolBar(ID_KDLG_TOOLBAR)->setItemEnabled(id_,false);
}

void CKDevelop::slotNewStatus()
{
  int config;
  config = edit_widget->config();
  statusBar()->changeItem(config & cfOvr ? "OVR" : "INS",ID_STATUS_INS_OVR);
  // set new caption... maybe the file content is changed
  setMainCaption();
}

void CKDevelop::slotCPPMarkStatus(KWriteView *,bool bMarked)
{

  int item=s_tab_view->getCurrentTab();
  if (item==CPP)
  {
    if(bMarked){
      enableCommand(ID_EDIT_CUT);
      enableCommand(ID_EDIT_COPY);
    }
    else{
      disableCommand(ID_EDIT_CUT);
      disableCommand(ID_EDIT_COPY);
    }
  }		
}

void CKDevelop::slotHEADERMarkStatus(KWriteView *, bool bMarked)
{
  int item=s_tab_view->getCurrentTab();
  if (item==HEADER)
  {
      if(bMarked){
        enableCommand(ID_EDIT_CUT);
        enableCommand(ID_EDIT_COPY);
      }
      else{
        disableCommand(ID_EDIT_CUT);
        disableCommand(ID_EDIT_COPY);
      }		
  }
}

void CKDevelop::slotBROWSERMarkStatus(KHTMLView *, bool bMarked)
{
  int item=s_tab_view->getCurrentTab();
  if (item==BROWSER)
  {
      if(bMarked){
        enableCommand(ID_EDIT_COPY);
      }
      else{
        disableCommand(ID_EDIT_COPY);
      }		
  }
}

void CKDevelop::slotClipboardChanged(KWriteView *, bool bContents)
{
  int item=s_tab_view->getCurrentTab();
  QString text=QApplication::clipboard()->text();
  if(!bContents || item==BROWSER || item==TOOLS)
    disableCommand(ID_EDIT_PASTE);
  else
    enableCommand(ID_EDIT_PASTE);
}

void CKDevelop::slotNewLineColumn()
{
  QString linenumber;
  linenumber.sprintf(i18n("Line: %d Col: %d"), 
     			edit_widget->currentLine() +1,
			edit_widget->currentColumn() +1);
  statusBar()->changeItem(linenumber.data(), ID_STATUS_LN_CLM);
} 
void CKDevelop::slotNewUndo(){
  int state;
  state = edit_widget->undoState();
  //undo
  if(state & 1){
    enableCommand(ID_EDIT_UNDO);
  }
  else{
    disableCommand(ID_EDIT_UNDO);
  }
  //redo
  if(state & 2){
    enableCommand(ID_EDIT_REDO);
  }
  else{
    disableCommand(ID_EDIT_REDO);
  }
  
}


void CKDevelop::slotURLSelected(KHTMLView* ,const char* url,int,const char*){
//	enableCommand(ID_HELP_BROWSER_STOP);
  if(!bKDevelop)
    switchToKDevelop();
  showOutputView(false);
  s_tab_view->setCurrentTab(BROWSER);
  browser_widget->setFocus();
  QString url_str = url;
  if(url_str.contains("kdevelop/search_result.html") != 0){
    browser_widget->showURL(url,true); // with reload if equal
  }
  else{
    browser_widget->showURL(url); // without reload if equal
  }

/*  if (!history_list.isEmpty()){
    enableCommand(ID_HELP_BACK);
  }
*/

  QString str = history_list.current();
  //if it's a url-request from the search result jump to the correct point
  if (str.contains("kdevelop/search_result.html")){
    prev_was_search_result=true; // after this time, jump to the searchkey
  }
/*  // insert into the history-list
  if(QString(url).left(7) != "http://"){ // http aren't added to the history list

    int cur =  history_list.at(); // get the current index
    if(cur == -1){
      history_list.append(url);
			history_title_list.append(browser_widget->currentTitle());
    }
    else{
      history_list.insert(cur+1,url);
      history_title_list.insert(cur+1, browser_widget->currentTitle());
    }
  }
*/	
}

void CKDevelop::slotURLonURL(KHTMLView*, const char *url )
{
	if ( url )
	{
		statusBar()->changeItem(url,ID_STATUS_MSG);
	}
	else
	{
		statusBar()->changeItem(i18n("Ready."), ID_STATUS_MSG);
	}
}

void CKDevelop::slotDocumentDone( KHTMLView *_view ){
  QString actualURL=browser_widget->currentURL();
  QString actualTitle=browser_widget->currentTitle();
  int cur =  history_list.at()+1; // get the current index
  int found =  history_list.find(actualURL); // get the current index
  int pos = actualURL.findRev('#');
  QString url_wo_ref=actualURL; // without ref

  if(prev_was_search_result){
    browser_widget->findTextBegin();
    browser_widget->findTextNext(QRegExp(doc_search_text));
  }

  if (s_tab_view->getCurrentTab()==BROWSER)
     setMainCaption(BROWSER);

  if (pos!=-1)
   url_wo_ref = actualURL.left(pos);

  // insert into the history-list
  // the following if-statement isn't necessary, because
  //   slotDocumentDone isn't called in the other cases [use of KFMclient for non file://....htm(l)]
  if(actualURL.left(7) != "http://" && url_wo_ref.right(4).find("htm", FALSE)>-1){
   // http aren't added to the history list ...

   if (found == -1)
   {
    if(cur == 0 ){
      history_list.append(actualURL);
      history_title_list.append(actualTitle);
    }
    else{
      history_list.insert(cur,actualURL);
      history_title_list.insert(cur, actualTitle);
    }
   }
   else
   {
     // the desired URL was already found in the list

     if (actualURL.contains("kdevelop/search_result.html") &&
	history_title_list.at(found)!=actualTitle)
     {
         // this means... a new search_result.html is selected and an old one
         // was found in list
         //   so append it at the end
         history_list.remove(found);
         history_title_list.remove(found);
         // append now the new one
         cur=history_list.count();
         history_list.insert(cur,actualURL);
         history_title_list.insert(cur, actualTitle);
     }
     else
     if (prev_was_search_result)
      {
         // this means... sort the found entry after the search_result.html-entry
         //   so we can always use the back button to get the last search results
         history_list.remove(found);
         history_title_list.remove(found);
         // correct cur after removing a list element
         if (found<cur)
           cur--;
         history_list.insert(cur,actualURL);
         history_title_list.insert(cur, actualTitle);
      }
      else
      {
         cur=found;
      }
   }

   // set now the pointer of the history list
   history_list.at(cur);

   // reorganize the prev- and the next-historylist
   history_next->clear();
   history_prev->clear();
		
   int i;
   for ( i =0 ; i < cur; i++){
       history_prev->insertItem(history_title_list.at(i));
   }

   for (i = cur+1 ; i < (int) history_list.count(); i++){
       history_next->insertItem(history_title_list.at(i));
   }

   // disable the back button if necessary
   if (cur == 0){ // no more backwards
 	disableCommand(ID_HELP_BACK);
   }
   else {
 	enableCommand(ID_HELP_BACK);
   }

   // disable the forward button if necessary
   if (cur >= ((int) history_list.count())-1){ // no more forwards
 	disableCommand(ID_HELP_FORWARD);
   }
   else {
 	enableCommand(ID_HELP_FORWARD);
   }

  }

  prev_was_search_result=false;
  disableCommand(ID_HELP_BROWSER_STOP);
}

void CKDevelop::slotReceivedStdout(KProcess*,char* buffer,int buflen){
  int x,y;
  messages_widget->cursorPosition(&x,&y);
  QString str(buffer,buflen+1);
  messages_widget->insertAt(str,x,y);
  o_tab_view->setCurrentTab(MESSAGES);
  // QString str1 = messages_widget->text();

//   if(error_parser->getMode() == CErrorMessageParser::MAKE){
    
//     error_parser->parseInMakeMode(&str1,prj->getProjectDir() + prj->getSubDir());
//   }
//   if(error_parser->getMode() == CErrorMessageParser::SGML2HTML){
//     error_parser->parseInSgml2HtmlMode(&str1,prj->getProjectDir() + prj->getSubDir() + "/docs/en/" + prj->getSGMLFile());
//   }

//   //enable/disable the menus/toolbars
//   if(error_parser->hasNext()){
//     enableCommand(ID_VIEW_NEXT_ERROR);
//   }
//   else{
//     disableCommand(ID_VIEW_NEXT_ERROR);
//   }
  
//   if(error_parser->hasPrev()){
//     enableCommand(ID_VIEW_PREVIOUS_ERROR);
//   }
//   else{
//     disableCommand(ID_VIEW_PREVIOUS_ERROR);
//   }
}
void CKDevelop::slotReceivedStderr(KProcess*,char* buffer,int buflen){
  int x,y;
  messages_widget->cursorPosition(&x,&y);
  QString str(buffer,buflen+1);
  messages_widget->insertAt(str,x,y);
  o_tab_view->setCurrentTab(MESSAGES);
  // QString str1 = messages_widget->text();
//   if(error_parser->getMode() == CErrorMessageParser::MAKE){
//     error_parser->parseInMakeMode(&str1,prj->getProjectDir() + prj->getSubDir());
//   }
//   if(error_parser->getMode() == CErrorMessageParser::SGML2HTML){
//     error_parser->parseInSgml2HtmlMode(&str1,prj->getProjectDir() + prj->getSubDir() + "/docs/en/" + prj->getSGMLFile());
//   }

//   //enable/disable the menus/toolbars
//   if(error_parser->hasNext()){
//     enableCommand(ID_VIEW_NEXT_ERROR);
//   }
//   else{
//     disableCommand(ID_VIEW_NEXT_ERROR);
//   }
  
//   if(error_parser->hasPrev()){
//     enableCommand(ID_VIEW_PREVIOUS_ERROR);
//   }
//   else{
//     disableCommand(ID_VIEW_PREVIOUS_ERROR);
//   }
}
void CKDevelop::slotApplReceivedStdout(KProcess*,char* buffer,int buflen){
  int x,y;
  showOutputView(true);
  stdin_stdout_widget->cursorPosition(&x,&y);
  QString str(buffer,buflen+1);
  stdin_stdout_widget->insertAt(str,x,y);
}
void CKDevelop::slotApplReceivedStderr(KProcess*,char* buffer,int buflen){
  int x,y;
  showOutputView(true);
  stderr_widget->cursorPosition(&x,&y);
  QString str(buffer,buflen+1);
  stderr_widget->insertAt(str,x,y);
}


void CKDevelop::slotSearchReceivedStdout(KProcess* proc,char* buffer,int buflen){
  QString str(buffer,buflen+1);
  search_output = search_output + str;
}
void CKDevelop::slotSearchProcessExited(KProcess*){
	disableCommand(ID_HELP_BROWSER_STOP);
  //  cerr << search_output;
  int pos=0;
  int nextpos=0;
  QStrList list;
  QStrList sort_list;
  QString str;
  QString found_str;
  int i=0;
  int max=0;

  while((nextpos = search_output.find('\n',pos)) != -1){
    str = search_output.mid(pos,nextpos-pos);
    list.append(str);
    pos = nextpos+1;
  }
  if (list.isEmpty()){

     KMsgBox::message(0,i18n("Not found!"),"\"" + doc_search_display_text + i18n("\" not found in documentation!"),KMsgBox::INFORMATION);
    return;
  }

  // //lets sort it a little bit
   for(;i<30;i++){
    max =0;
    found_str = "";
    for(str = list.first();str != 0;str = list.next()){
      if (searchToolGetNumber(str) >= max){
	found_str = str.copy();
	max = searchToolGetNumber(str);
      }
    }
    if(found_str != ""){
      sort_list.append(found_str);
      list.remove(found_str);
    }

  }

   QString filename = KApplication::localkdedir()+"/share/apps" + "/kdevelop/search_result.html";
   QFile file(filename);
   QTextStream stream(&file);
   file.open(IO_WriteOnly);

   stream << "<HTML>";
   stream << "<HEAD><TITLE> - " << i18n("Search for: ") << doc_search_display_text;
   stream << "</TITLE></HEAD><BODY BGCOLOR=\"#ffffff\"><BR> <TABLE><TR><TH>";
   stream << i18n("Title") << "<TH>" << i18n("Hits") << "\n";
   QString numstr;
   for(str = sort_list.first();str != 0;str = sort_list.next() ){
     stream << "<TR><TD><A HREF=\""+searchToolGetURL(str)+"\">"+
			   searchToolGetTitle(str)+"</A><TD>"+
			   numstr.setNum(searchToolGetNumber(str)) + "\n";
   }

   stream << "\n</TABLE></BODY></HTML>";

   file.close();
   slotURLSelected(browser_widget,"file:" + filename,1,"test");

}
QString CKDevelop::searchToolGetTitle(QString str){
  int pos = str.find(' ');
  pos = str.find(' ',pos);
  int end_pos = str.find(':',pos);
  return str.mid(pos,end_pos-pos);
}
QString CKDevelop::searchToolGetURL(QString str){
  int pos = str.find(' ');
  return str.left(pos);
}
int CKDevelop::searchToolGetNumber(QString str){
  int pos =str.findRev(':');
  QString sub = str.right((str.length()-pos-2));
  return sub.toInt();
}
/*
void CKDevelop::slotKeyPressedOnStdinStdoutWidget(int key){
  char a = key;
  appl_process.writeStdin(&a,1);
}
*/
void CKDevelop::slotClickedOnMessagesWidget(){
  TErrorMessageInfo info;
  int x,y;

  messages_widget->cursorPosition(&x,&y);
  info = error_parser->getInfo(x+1);
  if(info.filename != ""){
    if(!bKDevelop)
      switchToKDevelop();
    messages_widget->setCursorPosition(info.makeoutputline,0);
    switchToFile(info.filename,info.errorline-1);
  }
  else{
     XBell(kapp->getDisplay(),100); // not a next found, beep
  }
    // switchToFile(error_filename);
//     edit_widget->setCursorPosition(error_line-1,0);
//     edit_widget->setFocus();
  // int x,y;
//   int error_line;
//   QString text;
//   QString error_line_str;
//   QString error_filename;
//   int pos1,pos2; // positions in the string
//   QRegExp reg(":[0-9]*:"); // is it an error line?, I hope it works

  
 //  text = messages_widget->textLine(x);
//   if((pos1=reg.match(text)) == -1) return; // not an error line

//   // extract the error-line
//   pos2 = text.find(':',pos1+1);
//   error_line_str = text.mid(pos1+1,pos2-pos1-1);
//   error_line = error_line_str.toInt();

//   // extract the filename
//   pos2 = text.findRev(' ',pos1);
//   if (pos2 == -1) {
//     pos2 = 0; // the filename is at the begining of the string
//   }
//   else { pos2++; }

//   error_filename = text.mid(pos2,pos1-pos2);

//   // switch to the file
//   if (error_filename.find('/') == -1){ // it is a file outer the projectdir ?
//     error_filename = prj->getProjectDir() + prj->getSubDir() + error_filename;
//   }
//   if (QFile::exists(error_filename)){
    
    //  }


}
void CKDevelop::slotProcessExited(KProcess* proc){
  setToolMenuProcess(true);
  slotStatusMsg(i18n("Ready."));
  bool ready = true;
  QString result="";
  if (proc->normalExit()) {
    
    result= ((proc->exitStatus()) ? i18n("*** failed ***\n") : 
          i18n("*** success ***\n"));
    if ( proc== &appl_process)
      result.sprintf(i18n("*** exit-code: %i ***\n"), 
		     proc->exitStatus());
    
    if (next_job == make_cmd){ // rest from the rebuild all
      QDir::setCurrent(prj->getProjectDir() + prj->getSubDir());
      process.clearArguments();
      if(!prj->getMakeOptions().isEmpty()){
      	process << make_cmd << prj->getMakeOptions();
      }
      else{
      	process << make_cmd;
      }
      setToolMenuProcess(false);
      process.start(KProcess::NotifyOnExit,KProcess::AllOutput);
      next_job = "";
      ready=false;
    }
    if ((next_job == "run"  || next_job == "run_with_args") && process.exitStatus() == 0){ 
      // rest from the buildRun
      appl_process.clearArguments();
      QDir::setCurrent(prj->getProjectDir() + prj->getSubDir());
      stdin_stdout_widget->clear();
      stderr_widget->clear();
      QString args = prj->getExecuteArgs();
      QString program = prj->getBinPROGRAM().lower();
      
      
      if(next_job == "run_with_args"){
	if(!args.isEmpty()){
	  program = prj->getBinPROGRAM().lower() + " "+args;
	}
      }

      // Warning: not every user has the current directory in his path !
      if(prj->getProjectType() == "normal_cpp" || prj->getProjectType() == "normal_c"){
	o_tab_view->setCurrentTab(STDINSTDOUT);
	QString term = "xterm";
	QString exec_str = term + " -e sh -c './" +  program + "'";
	
	if(CToolClass::searchInstProgram("konsole")){
	  term = "konsole";
	}
	if(CToolClass::searchInstProgram("ksh")){
	  exec_str = term + " -e ksh -c './" + program + 
	    ";echo \"\n" + QString(i18n("Press Enter to continue!")) + "\";read'";
	}
	if(CToolClass::searchInstProgram("csh")){
	  exec_str = term +" -e csh -c './" + program + 
	    ";echo \"\n" + QString(i18n("Press Enter to continue!")) + "\";$<'";
	}
	if(CToolClass::searchInstProgram("tcsh")){
	  exec_str =  term +" -e tcsh -c './" + program + 
	    ";echo \"\n" + QString(i18n("Press Enter to continue!")) + "\";$<'";
	}
	if(CToolClass::searchInstProgram("bash")){
	  exec_str =  term +" -e bash -c './" + program + 
	    ";echo \"\n" + QString(i18n("Press Enter to continue!")) + "\";read'";
	}
	appl_process << exec_str;
	cerr << endl << "EXEC:" << exec_str;
      }
      else{
	appl_process << "./" + program;
	cerr << endl << "EXEC:" << "./" +program;
	o_tab_view->setCurrentTab(STDERR);
      }
      setToolMenuProcess(false);
      appl_process.start(KProcess::NotifyOnExit,KProcess::All);
      next_job = "";
      ready = false;
    }
      
    if (next_job == "refresh"){ // rest from the add projectfile
      refreshTrees();
    }
    next_job = "";
  }
  else {
    result= i18n("*** process exited with error(s) ***\n");
    next_job = "";
    
  }
  if (!result.isEmpty())
  {
     int x,y;
     messages_widget->cursorPosition(&x,&y);
     messages_widget->insertAt(result, x, y);
  }
  if (ready){ // start the error-message parser
      QString str1 = messages_widget->text();
      
      if(error_parser->getMode() == CErrorMessageParser::MAKE){
	  error_parser->parseInMakeMode(&str1,prj->getProjectDir() + prj->getSubDir());
      }
      if(error_parser->getMode() == CErrorMessageParser::SGML2HTML){
	  error_parser->parseInSgml2HtmlMode(&str1,prj->getProjectDir() + prj->getSubDir() + "/docs/en/" + prj->getSGMLFile());
      }
      //enable/disable the menus/toolbars
      if(error_parser->hasNext()){
	  enableCommand(ID_VIEW_NEXT_ERROR);
      }
      else{
	  disableCommand(ID_VIEW_NEXT_ERROR);
      }
  }
  if(beep && ready){
      XBell(kapp->getDisplay(),100); //beep :-)
      beep = false;
  }
  
}


void CKDevelop::slotTTabSelected(int item){
  if(item == DOC ){
    // disable the outputview
    showOutputView(false);
  }
}
void CKDevelop::slotSTabSelected(int item){
  lasttab = s_tab_view->getCurrentTab();

  if (item == HEADER || item == CPP)
  {
   // enableCommand(ID_FILE_SAVE);  is handled by setMainCaption()
  enableCommand(ID_FILE_SAVE_AS);
  enableCommand(ID_FILE_CLOSE);

  enableCommand(ID_FILE_PRINT);

  QString text=QApplication::clipboard()->text();
  if(text.isEmpty())
    disableCommand(ID_EDIT_PASTE);
  else
    enableCommand(ID_EDIT_PASTE);

  enableCommand(ID_EDIT_INSERT_FILE);
  enableCommand(ID_EDIT_SEARCH);
  enableCommand(ID_EDIT_REPEAT_SEARCH);
  enableCommand(ID_EDIT_REPLACE);
  enableCommand(ID_EDIT_SPELLCHECK);
  enableCommand(ID_EDIT_INDENT);
  enableCommand(ID_EDIT_UNINDENT);
  enableCommand(ID_EDIT_SELECT_ALL);
  enableCommand(ID_EDIT_DESELECT_ALL);
  enableCommand(ID_EDIT_INVERT_SELECTION);

  }

  if (item == HEADER){
    if(bAutoswitch && t_tab_view->getCurrentTab()==DOC){	
      if ( bDefaultCV)
				t_tab_view->setCurrentTab(CV);
      else
				t_tab_view->setCurrentTab(LFV);
    }
    disableCommand(ID_BUILD_COMPILE_FILE);
    edit_widget = header_widget;
    edit_widget->setFocus();
    slotNewUndo();
    slotNewStatus();
//    setMainCaption();  is called by slotNewStatus()
    slotNewLineColumn();
  }
  if (item == CPP){
    if(bAutoswitch && t_tab_view->getCurrentTab()==DOC){	
      if ( bDefaultCV)
				t_tab_view->setCurrentTab(CV);
      else
				t_tab_view->setCurrentTab(LFV);
    }
    if(project && build_menu->isItemEnabled(ID_BUILD_MAKE)){
      enableCommand(ID_BUILD_COMPILE_FILE);
    }
    edit_widget = cpp_widget;
    edit_widget->setFocus();
    slotNewUndo();
    slotNewStatus();
//    setMainCaption();  is called by slotNewStatus()
    slotNewLineColumn();
  }

  if (item == HEADER || item == CPP)
  {
    int state;
    state = edit_widget->undoState();
    //undo
    if(state & 1)
      enableCommand(ID_EDIT_UNDO);
    else
      disableCommand(ID_EDIT_UNDO);
    //redo
    if(state & 2)
      enableCommand(ID_EDIT_REDO);
    else
      disableCommand(ID_EDIT_REDO);

    QString str = edit_widget->markedText();
    if(str.isEmpty()){
      disableCommand(ID_EDIT_CUT);
      disableCommand(ID_EDIT_COPY);
    }
    else{
      enableCommand(ID_EDIT_CUT);
      enableCommand(ID_EDIT_COPY);
    }		
  }

  if(item == BROWSER || item == TOOLS)
  {
    disableCommand(ID_BUILD_COMPILE_FILE);

    disableCommand(ID_FILE_SAVE);
    disableCommand(ID_FILE_SAVE_AS);
    disableCommand(ID_FILE_CLOSE);

    disableCommand(ID_FILE_PRINT);

    disableCommand(ID_EDIT_UNDO);
    disableCommand(ID_EDIT_REDO);
    disableCommand(ID_EDIT_CUT);
    disableCommand(ID_EDIT_PASTE);
    disableCommand(ID_EDIT_INSERT_FILE);
    disableCommand(ID_EDIT_SEARCH);
    disableCommand(ID_EDIT_REPEAT_SEARCH);
    disableCommand(ID_EDIT_REPLACE);
    disableCommand(ID_EDIT_SPELLCHECK);
    disableCommand(ID_EDIT_INDENT);
    disableCommand(ID_EDIT_UNINDENT);
    disableCommand(ID_EDIT_SELECT_ALL);
    disableCommand(ID_EDIT_DESELECT_ALL);
    disableCommand(ID_EDIT_INVERT_SELECTION);
  }

  if(item == BROWSER){
    if(bAutoswitch)
      t_tab_view->setCurrentTab(DOC);
    browser_widget->setFocus();

    if (browser_widget->isTextSelected())
      enableCommand(ID_EDIT_COPY);
    else
      disableCommand(ID_EDIT_COPY);

    setMainCaption(BROWSER);
  }

  if(item == TOOLS){
    disableCommand(ID_EDIT_COPY);
    setMainCaption(TOOLS);
  }
  //  s_tab_current = item;

}

void CKDevelop::slotMenuBuffersSelected(int id){
  TEditInfo* info;

  for(info=edit_infos.first();info != 0;info=edit_infos.next()){
    if (info->id == id){
      switchToFile(info->filename);
      return; // if found than return
    }
  }
}


void CKDevelop::slotLogFileTreeSelected(QString file){
  switchToFile(file);
}

void CKDevelop::slotRealFileTreeSelected(QString file){
  switchToFile(file);
}

void CKDevelop::slotUpdateFileFromVCS(QString file){
    switchToFile(file,false,false); // force not reload and no messagebox if modified on disc, because slotFileSave() will do it
    slotFileSave();
    prj->getVersionControl()->update(file);
    switchToFile(file,true,false);
}
void CKDevelop::slotCommitFileToVCS(QString file){
    switchToFile(file,false,false);
    slotFileSave();
    prj->getVersionControl()->commit(file);
    switchToFile(file,true,false);
}

void CKDevelop::slotUpdateDirFromVCS(QString dir){
    slotFileSaveAll();

    prj->getVersionControl()->update(dir);
    TEditInfo* actual_info;
    
    QListIterator<TEditInfo> it(edit_infos); // iterator for edit_infos list

    for ( ; it.current(); ++it ) {
	actual_info = it.current();
	QFileInfo file_info(actual_info->filename);
	if(actual_info->last_modified != file_info.lastModified()){ // reload only changed files
	    switchToFile(actual_info->filename,true,false); //force reload, no modified on disc messagebox
	}
    }
}

void CKDevelop::slotCommitDirToVCS(QString dir){
    slotFileSaveAll();
    prj->getVersionControl()->commit(dir);

    TEditInfo* actual_info;
    
    QListIterator<TEditInfo> it(edit_infos); // iterator for edit_infos list

    for ( ; it.current(); ++it ) {
	actual_info = it.current();
	QFileInfo file_info(actual_info->filename);
	if(actual_info->last_modified != file_info.lastModified()){ // reload only changed files
	    switchToFile(actual_info->filename,true,false); //force reload, no modified on disc messagebox
	}
    }
    
}
void CKDevelop::slotDocTreeSelected(QString url_file){
  if(url_file == "API-Documentation"){
    slotHelpAPI();
    return;
  }
  if(url_file == "User-Manual"){
    slotHelpManual();
    return;
  }
  QString text = doc_tree->selectedText();
  
  if(!QFile::exists(url_file)){
    if( text == i18n("Qt-Library")){
      if(KMsgBox::yesNo(0,i18n("File not found!"),"KDevelop couldn't find the Qt documentation.\n Do you want to set the correct path?",KMsgBox::INFORMATION) == 1) {
				slotOptionsKDevelop();
      }
      return;
    }
    if(text ==  i18n("KDE-Core-Library") || text == i18n("KDE-UI-Library") ||
       text == i18n("KDE-KFile-Library") || text == i18n("KDE-KHTMLW-Library") ||
       text == i18n("KDE-KFM-Library") || text == i18n("KDE-KDEutils-Library") ||
       text == i18n("KDE-KAB-Library") || text == i18n("KDE-KSpell-Library")){
      if(KMsgBox::yesNo(0,i18n("File not found!"),"KDevelop couldn't find the KDE API-Documentation.\nDo you want to generate it now?",KMsgBox::INFORMATION) == 1) {
				slotOptionsUpdateKDEDocumentation();
      }
      return;
    }
  }
  slotURLSelected(browser_widget,"file:"+ url_file,1,"test");
  
}

void CKDevelop::slotTCurrentTab(int item){
    t_tab_view->setCurrentTab(item);
}

void CKDevelop::slotSCurrentTab(int item){
    s_tab_view->setCurrentTab(item);
}

void CKDevelop::slotToggleLast() {
  if ( lasttab != s_tab_view->getCurrentTab() )
    s_tab_view->setCurrentTab( lasttab );
  else
    switchToFile( lastfile );
}

void CKDevelop::slotBufferMenu( const QPoint& point ) {
  menu_buffers->popup( point );
}

void CKDevelop::slotGrepDialogItemSelected(QString filename,int linenumber){
  switchToFile(filename,linenumber);
}

void CKDevelop::slotToolbarClicked(int item){
  switch (item) {
  case ID_FILE_NEW:
    slotFileNew();
    break;
  case ID_PROJECT_OPEN:
    slotProjectOpen();
    break;
  case ID_FILE_OPEN:
    slotFileOpen();
    break;
  case ID_FILE_SAVE:
    slotFileSave();
    break;
  case ID_KDLG_FILE_SAVE:
    kdlgedit->slotFileSave();
    break;
  case ID_FILE_SAVE_ALL:
    slotFileSaveAll();
    break;
  case ID_FILE_PRINT:
    slotFilePrint();
    break;
  case ID_EDIT_UNDO:
    slotEditUndo();
    break;
  case ID_KDLG_EDIT_UNDO:
    kdlgedit->slotEditUndo();
    break;
  case ID_EDIT_REDO:
    slotEditRedo();
    break;
  case ID_KDLG_EDIT_REDO:
    kdlgedit->slotEditRedo();
    break;
  case ID_EDIT_COPY:
    slotEditCopy();
    break;
  case ID_KDLG_EDIT_COPY:
    kdlgedit->slotEditCopy();
    break;
  case ID_EDIT_PASTE:
    slotEditPaste();
    break;
  case ID_KDLG_EDIT_PASTE:
    kdlgedit->slotEditPaste();
    break;
  case ID_EDIT_CUT:
    slotEditCut();
    break;
  case ID_KDLG_EDIT_CUT:
    kdlgedit->slotEditCut();
    break;
  case ID_VIEW_REFRESH:
    slotViewRefresh();
    break;
	case ID_VIEW_TREEVIEW:
		slotViewTTreeView();
		break;
	case ID_VIEW_OUTPUTVIEW:
		slotViewTOutputView();
		break;
  case ID_BUILD_COMPILE_FILE:
  	slotBuildCompileFile();
  	break;
  case ID_BUILD_MAKE:
    slotBuildMake();
    break;
  case ID_BUILD_REBUILD_ALL:
  	slotBuildRebuildAll();
  	break;
  case ID_BUILD_DEBUG:
    slotBuildDebug();
    break;
  case ID_BUILD_RUN:
    slotBuildRun();
    break;
  case ID_BUILD_STOP:
    slotBuildStop();
    break;
  case ID_TOOLS_KDLGEDIT:
    switchToKDlgEdit();
    break;
  case ID_KDLG_TOOLS_KDEVELOP:
    switchToKDevelop();
    break;
  case ID_HELP_BACK:
    slotHelpBack();
    break;
  case ID_HELP_FORWARD:
    slotHelpForward();
    break;
	case ID_HELP_BROWSER_RELOAD:
		slotHelpBrowserReload();
		break;
	case ID_HELP_BROWSER_STOP:
		browser_widget->cancelAllRequests();
	  shell_process.kill();
		disableCommand(ID_HELP_BROWSER_STOP);
		break;
	case ID_HELP_CONTENTS:
		slotHelpContents();
		break;
  case ID_HELP_SEARCH_TEXT:
    slotHelpSearchText();
    break;
  case ID_HELP_SEARCH:
    slotHelpSearch();
    break;
  case ID_KDLG_BUILD_GENERATE:
    kdlgedit->slotBuildGenerate();
    break;
  case ID_CV_WIZARD:
    // Make the button toggle between declaration and definition.
    if(cv_decl_or_impl){
      slotClassbrowserViewDeclaration();
      cv_decl_or_impl=false;
    }
    else{
      slotClassbrowserViewDefinition();
      cv_decl_or_impl=true;
    }		
    break;
  }
}

void CKDevelop::statusCallback(int id_){
  switch(id_){
    ON_STATUS_MSG(ID_FILE_NEW,                              i18n("Creates a new file"))
    ON_STATUS_MSG(ID_FILE_OPEN,   							            i18n("Opens an existing file"))
  ON_STATUS_MSG(ID_FILE_CLOSE,       						          i18n("Closes the actual file"))

  ON_STATUS_MSG(ID_FILE_SAVE,        						          i18n("Save the actual document"))
  ON_STATUS_MSG(ID_FILE_SAVE_AS,     						          i18n("Save the document as..."))
  ON_STATUS_MSG(ID_FILE_SAVE_ALL,    						          i18n("Save all changed files"))

  ON_STATUS_MSG(ID_FILE_PRINT,       						          i18n("Prints the current document"))

//  ON_STATUS_MSG(ID_FILE_CLOSE_WINDOW,i18n("Closes the current window"))

  ON_STATUS_MSG(ID_FILE_QUIT,        						          i18n("Exits the program"))

	ON_STATUS_MSG(ID_EDIT_UNDO,															i18n("Reverts the last editing step"))
	ON_STATUS_MSG(ID_EDIT_REDO,															i18n("Re-execute the last undone step"))
	
  ON_STATUS_MSG(ID_EDIT_CUT,                     			    i18n("Cuts the selected section and puts it to the clipboard"))
  ON_STATUS_MSG(ID_EDIT_COPY,                    			    i18n("Copys the selected section to the clipboard"))
  ON_STATUS_MSG(ID_EDIT_PASTE,                   			    i18n("Pastes the clipboard contents to actual position"))

  ON_STATUS_MSG(ID_EDIT_INSERT_FILE,             			    i18n("Inserts a file at the current position"))

  ON_STATUS_MSG(ID_EDIT_SEARCH,                  			    i18n("Searchs the file for an expression"))
  ON_STATUS_MSG(ID_EDIT_REPEAT_SEARCH,           			    i18n("Repeats the last search"))
  ON_STATUS_MSG(ID_EDIT_REPLACE,                 			    i18n("Searchs and replace expression"))
	ON_STATUS_MSG(ID_EDIT_SEARCH_IN_FILES,									i18n("Opens the search in files dialog to search for expressions over several files"))
	
	ON_STATUS_MSG(ID_EDIT_INDENT,														i18n("Moves the selection to the right"))
	ON_STATUS_MSG(ID_EDIT_UNINDENT,													i18n("Moves the selection to the left"))
	
  ON_STATUS_MSG(ID_EDIT_SELECT_ALL,              			    i18n("Selects the whole document contents"))
	ON_STATUS_MSG(ID_EDIT_DESELECT_ALL,											i18n("Deselects the whole document contents"))
	ON_STATUS_MSG(ID_EDIT_INVERT_SELECTION,									i18n("Inverts the current selection"))

	
  ON_STATUS_MSG(ID_VIEW_GOTO_LINE,               			    i18n("Goes to Line Number..."))
	ON_STATUS_MSG(ID_VIEW_NEXT_ERROR,												i18n("Switches to the file and line the next error was reported"))
	ON_STATUS_MSG(ID_VIEW_PREVIOUS_ERROR,										i18n("Switches to the file and line the previous error was reported"))
	
  ON_STATUS_MSG(ID_VIEW_TREEVIEW,              				    i18n("Enables / disables the treeview"))
  ON_STATUS_MSG(ID_VIEW_OUTPUTVIEW,            				    i18n("Enables / disables the outputview"))

  ON_STATUS_MSG(ID_VIEW_TOOLBAR,                  			  i18n("Enables / disables the standard toolbar"))
  ON_STATUS_MSG(ID_VIEW_BROWSER_TOOLBAR,       				    i18n("Enables / disables the browser toolbar"))
  ON_STATUS_MSG(ID_VIEW_STATUSBAR,             				    i18n("Enables / disables the statusbar"))

  ON_STATUS_MSG(ID_VIEW_REFRESH,                			    i18n("Refreshes current view"))
  ON_STATUS_MSG(ID_VIEW_IN_KFM,					   								i18n("Opens the current document in the KFM browser"))
  ON_STATUS_MSG(ID_PROJECT_KAPPWIZARD,            			  i18n("Generates a new project with Application Wizard"))
  ON_STATUS_MSG(ID_PROJECT_OPEN,			            	      i18n("Opens an existing project"))
  ON_STATUS_MSG(ID_PROJECT_CLOSE,                 			  i18n("Closes the current project"))
  ON_STATUS_MSG(ID_PROJECT_ADD_FILE_EXIST,        			  i18n("Adds an existing file to the project"))
	ON_STATUS_MSG(ID_PROJECT_ADD_NEW_TRANSLATION_FILE,			i18n("Adds a new language for internationalization to the project"))
  ON_STATUS_MSG(ID_PROJECT_REMOVE_FILE,           			  i18n("Removes file from the project"))

  ON_STATUS_MSG(ID_PROJECT_NEW_CLASS,             			  i18n("Creates a new Class frame structure and files"))

  ON_STATUS_MSG(ID_PROJECT_FILE_PROPERTIES,       			  i18n("Shows the current file properties"))
  ON_STATUS_MSG(ID_PROJECT_OPTIONS,               			  i18n("Sets project and compiler options"))
	ON_STATUS_MSG(ID_PROJECT_MESSAGES,											i18n("Invokes make to create the message file by extracting all i18n() macros"))
  ON_STATUS_MSG(ID_PROJECT_MAKE_PROJECT_API,        			i18n("Creates the Project's API with KDoc"))
  ON_STATUS_MSG(ID_PROJECT_MAKE_USER_MANUAL,        			i18n("Creates the Project's User Manual with the sgml-file"))
	ON_STATUS_MSG(ID_PROJECT_MAKE_DISTRIBUTION,							i18n("Creates distribution packages from the current project"))
	ON_STATUS_MSG(ID_PROJECT_MAKE_DISTRIBUTION_SOURCE_TGZ,  i18n("Creates a tar.gz file from the current project sources"))

  ON_STATUS_MSG(ID_BUILD_COMPILE_FILE,                    i18n("Compiles the current sourcefile"))
  ON_STATUS_MSG(ID_KDLG_BUILD_GENERATE,                   i18n("Generates the sourcefiles for the dialog"))
  ON_STATUS_MSG(ID_BUILD_MAKE,                    			  i18n("Invokes make-command"))
  ON_STATUS_MSG(ID_BUILD_REBUILD_ALL,             			  i18n("Rebuilds the program"))
  ON_STATUS_MSG(ID_BUILD_CLEAN_REBUILD_ALL,       			  i18n("Invokes make clean and rebuild all"))
  ON_STATUS_MSG(ID_BUILD_STOP,                    			  i18n("Stops make immediately"))
  ON_STATUS_MSG(ID_BUILD_RUN,                     			  i18n("Invokes make-command and runs the program"))
  ON_STATUS_MSG(ID_BUILD_RUN_WITH_ARGS,										i18n("Lets you set run-arguments to the binary and invokes the make-command"))

  ON_STATUS_MSG(ID_BUILD_DEBUG,                   			  i18n("Invokes make and KDbg debugging the binary"))
  ON_STATUS_MSG(ID_BUILD_DISTCLEAN,               			  i18n("Invokes make distclean and deletes all compiled files"))
  ON_STATUS_MSG(ID_BUILD_AUTOCONF,                			  i18n("Invokes automake and co."))
  ON_STATUS_MSG(ID_BUILD_CONFIGURE,               			  i18n("Invokes ./configure"))
 

  ON_STATUS_MSG(ID_TOOLS_KDLGEDIT,												i18n("Changes to the KDevelop dialogeditor"))
  ON_STATUS_MSG(ID_KDLG_TOOLS_KDEVELOP,										i18n("Changes to KDevelop project editor"))

  ON_STATUS_MSG(ID_OPTIONS_EDITOR,              			    i18n("Sets the Editor's behavoir"))
  ON_STATUS_MSG(ID_OPTIONS_EDITOR_COLORS,       			    i18n("Sets the Editor's colors"))
  ON_STATUS_MSG(ID_OPTIONS_SYNTAX_HIGHLIGHTING_DEFAULTS,  i18n("Sets the highlighting default colors"))
  ON_STATUS_MSG(ID_OPTIONS_SYNTAX_HIGHLIGHTING, 			    i18n("Sets the highlighting colors"))
  ON_STATUS_MSG(ID_OPTIONS_DOCBROWSER,     	  				    i18n("Configures the Browser options"))
	ON_STATUS_MSG(ID_OPTIONS_TOOLS_CONFIG_DLG,							i18n("Configures the Tools-Menu entries"))
  ON_STATUS_MSG(ID_OPTIONS_PRINT,       			            i18n("Configures printing options"))
  ON_STATUS_MSG(ID_OPTIONS_PRINT_ENSCRIPT,       	        i18n("Configures the printer to use enscript"))
  ON_STATUS_MSG(ID_OPTIONS_PRINT_A2PS,       			        i18n("Configures the printer to use a2ps"))
  ON_STATUS_MSG(ID_OPTIONS_KDEVELOP,              		    i18n("Configures KDevelop"))

  ON_STATUS_MSG(ID_BOOKMARKS_SET,													i18n("Sets a bookmark to the current window file"))
  ON_STATUS_MSG(ID_BOOKMARKS_ADD,													i18n("Adds a bookmark to the current window file"))
  ON_STATUS_MSG(ID_BOOKMARKS_CLEAR,												i18n("Clears the bookmark list for the current window"))

  ON_STATUS_MSG(ID_HELP_BACK,                      			  i18n("Switchs to last browser page"))
  ON_STATUS_MSG(ID_HELP_FORWARD,                   			  i18n("Switchs to next browser page"))

  ON_STATUS_MSG(ID_HELP_BROWSER_RELOAD,                   i18n("Reloads the current browser page"))
  ON_STATUS_MSG(ID_HELP_BROWSER_STOP,                   	i18n("Cancels the document request"))


  ON_STATUS_MSG(ID_HELP_SEARCH_TEXT,              				i18n("Searchs the selected text in the documentation"))
  ON_STATUS_MSG(ID_HELP_SEARCH,                           i18n("Lets you search individually for an expression"))

  ON_STATUS_MSG(ID_HELP_CONTENTS,                  			  i18n("Switchs to KDevelop's User Manual"))
  ON_STATUS_MSG(ID_HELP_TUTORIAL,													i18n("Switchs to the KDevelop Programming Handbook"))
  ON_STATUS_MSG(ID_HELP_TIP_OF_DAY,												i18n("Opens the Tip of the Day dialog with hints for using KDevelop"))

  ON_STATUS_MSG(ID_HELP_REFERENCE,                			  i18n("Switchs to the C/C++-Reference"))
  ON_STATUS_MSG(ID_HELP_QT_LIBRARY,                			  i18n("Switchs to the QT-Documentation"))
  ON_STATUS_MSG(ID_HELP_KDE_CORE_LIBRARY,          			  i18n("Switchs to the KDE-Core-Documentation"))
  ON_STATUS_MSG(ID_HELP_KDE_GUI_LIBRARY,           			  i18n("Switchs to the KDE-GUI-Documentation"))
  ON_STATUS_MSG(ID_HELP_KDE_KFILE_LIBRARY,          			i18n("Switchs to the KDE-File-Documentation"))
  ON_STATUS_MSG(ID_HELP_KDE_HTML_LIBRARY,          			  i18n("Switchs to the KDE-Html-Documentation"))
  ON_STATUS_MSG(ID_HELP_PROJECT_API,		           			  i18n("Switchs to the project's API-Documentation"))
  ON_STATUS_MSG(ID_HELP_USER_MANUAL,               			  i18n("Switchs to the project's User-Manual"))

  ON_STATUS_MSG(ID_HELP_HOMEPAGE,                 			  i18n("Enter the KDevelop Homepage"))
  ON_STATUS_MSG(ID_HELP_BUG_REPORT,			                 	i18n("Sends a bug-report email to the KDevelop Team"))

  ON_STATUS_MSG(ID_HELP_DLGNOTES,                 			  i18n("Some information about the dialog editor..."))
  ON_STATUS_MSG(ID_HELP_ABOUT,                    			  i18n("Programmer's Hall of Fame..."))

  ON_STATUS_MSG(ID_CV_WIZARD,															i18n("Switchs to declaration/implementation"))
  ON_STATUS_MSG(ID_CV_VIEW_DECLARATION,										i18n("Switchs to the method's declaration"))
  ON_STATUS_MSG(ID_CV_VIEW_DEFINITION,										i18n("Switchs to the method's definition"))
  ON_STATUS_MSG(ID_CV_GRAPHICAL_VIEW,											i18n("Opens the graphical inheritance tree"))
  ON_STATUS_MSG(ID_CV_CLASS_TOOL,													i18n("Opens the classtool dialog"))
  ON_STATUS_MSG(ID_CV_CLASS_BASE_CLASSES,									i18n("Displays the inherited classes of the current class"))
  ON_STATUS_MSG(ID_CV_CLASS_DERIVED_CLASSES,							i18n("Displays the classes who inherit the current class"))
  ON_STATUS_MSG(ID_CV_FOLDER_NEW,													i18n("Creates a new folder"))
  ON_STATUS_MSG(ID_CV_FOLDER_DELETE,											i18n("Deletes the current folder"))
  ON_STATUS_MSG(ID_CV_CLASS_DELETE,												i18n("Deletes the current class"))
  ON_STATUS_MSG(ID_CV_VIEW_CLASS_DECLARATION,							i18n("Goes to the class declaration"))
  ON_STATUS_MSG(ID_CV_METHOD_NEW,													i18n("Opens the New Method dialog"))
  ON_STATUS_MSG(ID_CV_METHOD_DELETE,											i18n("Deletes the current class method"))
  ON_STATUS_MSG(ID_CV_ATTRIBUTE_NEW,											i18n("Creates a new attribute for the current class"))
  ON_STATUS_MSG(ID_CV_ATTRIBUTE_DELETE,										i18n("Deletes the current class attribute"))
  ON_STATUS_MSG(ID_CV_IMPLEMENT_VIRTUAL,									i18n("Creates a virtual method"))
  ON_STATUS_MSG(ID_CV_ADD_SLOT_SIGNAL,										i18n("Adds a signal/slot mechanism"))
  
	ON_STATUS_MSG(ID_KDLG_FILE_CLOSE,												i18n("Closes the current dialog"))
	ON_STATUS_MSG(ID_KDLG_FILE_SAVE,												i18n("Saves the current dialog"))
	ON_STATUS_MSG(ID_KDLG_FILE_SAVE_AS,											i18n("Saves the current dialog under a new filename"))
	
	ON_STATUS_MSG(ID_KDLG_VIEW_PROPVIEW,										i18n("Enables/Disables the properties window"))
	ON_STATUS_MSG(ID_KDLG_VIEW_TOOLBAR,											i18n("Enables / disables the standard toolbar"))
	ON_STATUS_MSG(ID_KDLG_VIEW_STATUSBAR,										i18n("Enables / disables the statusbar"))

	ON_STATUS_MSG(ID_KDLG_VIEW_REFRESH,											i18n("Refreshes current view"))
	ON_STATUS_MSG(ID_KDLG_VIEW_GRID,												i18n("Sets the grid size of the editing widget grid snap"))

	// LFV popups
	ON_STATUS_MSG(ID_LFV_NEW_GROUP,													i18n("Lets you create a new logical file group"))
	ON_STATUS_MSG(ID_LFV_REMOVE_GROUP,											i18n("Removes the selected logical file group"))
 	ON_STATUS_MSG(ID_LFV_GROUP_PROP,												i18n("Shows the group's properties"))
	ON_STATUS_MSG(ID_LFV_SHOW_PATH_ITEM,										i18n("Displays the absolute / relative path"))
	ON_STATUS_MSG(ID_FILE_DELETE,														i18n("Deletes the selected file"))
	
	default: slotStatusMsg(i18n("Ready"));
	}
}
















