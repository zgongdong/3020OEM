#!/usr/bin/env python
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
#   %%version

# Python 2 and 3
from __future__ import print_function

import os
import sys
import errno
from shutil import copyfile
import stat
import argparse
import subprocess
from six.moves import tkinter_tkfiledialog as tkFileDialog
from six.moves import tkinter_messagebox as tkMessageBox
from six.moves.tkinter import Tk, TclError

xml_config_fragment="""\
<?xml version="1.0" encoding="UTF-8"?>
<project path="">
    <configurations>
        <configuration name="" options="">
            <property name="%s">%s</property>
        </configuration>
    </configurations>
</project>"""

class UI(object):
    def __init__(self, args):
        self.root = Tk()
        self.args = args

        # Make it invisible
        # - 0 size, top left corner, transparent.
        self.root.attributes('-alpha', 0.0)
        self.root.geometry('0x0+0+0')

        #try:
        #    qmde_icon = os.path.join(args.devkit, 'res', 'q_logo.ico')
        #    self.root.iconbitmap(bitmap=qmde_icon)
        #except TclError:
        #    pass

        # "Show" window again and lift it to top so it can get focus,
        # otherwise dialogs will end up behind other windows
        self.root.deiconify()
        self.root.lift()
        self.root.focus_force()

    def selectFolder(self, **options):
        """
        Show a folder selection window for the user to import
        an addon
        """
        return tkFileDialog.askdirectory(parent=self.root, **options)

    def success(self, action):
        message = 'Addon {} operation successful'.format(action.lower())
        print(message)
        tkMessageBox.showinfo(message=message)

    def cancel(self, action):
        message = 'Addon {} operation cancelled'.format(action.lower())
        print(message)
        tkMessageBox.showinfo(message=message)
        sys.exit(0)

    def fail(self, msg):
        print(msg)
        tkMessageBox.showerror(message=msg)
        sys.exit(1)

    def confirmAction(self, addon_name):
        proceed = True
        info = "About to import addon: {}\n\n".format(addon_name)
        info += "This operation will modify %s and %s files\n\n" % (self.args.project, self.args.workspace)
        question = "Proceed?"
        print(info + question)
        proceed = tkMessageBox.askokcancel('Attention!', info + question)
        print(proceed)
        return proceed

class AddonUtils(object):
    def __init__(self):
        from addon_utils import XPathCommand
        self.addon_utils_cmd_line = XPathCommand("addon")  

    def util_script_execute(self, cmds):
        from StringIO import StringIO
        old_stdout = sys.stdout
        result = StringIO()
        sys.stdout = result
        status = self.addon_utils_cmd_line.execute(cmds)        
        sys.stdout = old_stdout
        if status is -1:
            self.ui.fail("Script Error")
        return result.getvalue()

    def executePatchScript(self, input_file, templates_file):
        self.util_script_execute(["--input", input_file, "--merge", templates_file, "--output", input_file])

    def patchFile(self, merge_buffer, output_file):
        temp_addon_templates_file = os.path.join(os.path.dirname(output_file), 'addon_ext_temp.xml')
        f = open(temp_addon_templates_file,"w+")
        f.write(merge_buffer)
        f.close()        
        self.executePatchScript(output_file, temp_addon_templates_file)
        os.remove(temp_addon_templates_file)

    def readAppProjectConfigItem(self, config_path_type, project_file):
        config_items = self.util_script_execute(["--input", project_file, "--configs", config_path_type])
        config_items = config_items.replace('-D','',)
        config_items = config_items.replace('\n','',)
        return config_items

class Importer(object):
    def __init__(self, args):
        self.ui = UI(args)
        self.args = args
        
        # Set base paths (relativeness will be set after folder has been selected)
        self.app_templates_project_file = "tools/templates/addon_ext_appsp1_x2p.xml"
        self.app_templates_workspace_file = "tools/templates/addon_ext_appsp1_x2w.xml"
        self.ro_fs_templates_project_file = "tools/templates/addon_ext_ro_fs_x2p.xml"
        self.addon_utils_script = "adk/tools/addon_utils/addon_utils.py"
        self.ro_fs_project_file = "filesystems/ro_fs.x2p"
        self.addon_x2p_file = ""
        
        sys.path.append(os.path.dirname(self.args.project) + '/../../../adk/tools/addon_utils')
        self.addon_utils = AddonUtils()    
        
    def cancel(self):
        self.ui.cancel('import')

    def success(self):
        self.ui.success('import')

    def checkPrerequisiteExists(self, prerequisite):
        if not os.path.exists(prerequisite):
            self.ui.fail('no ' + prerequisite + ' found')
    
    def patchAddonProjectFile(self, config_path_type):
        app_paths = self.addon_utils.readAppProjectConfigItem(config_path_type, self.args.project)

        # "../../../../" or "..\..\..\..\" represents the relative path to src beneath ADK from loaded x2/x2w, 
        # so need to alter relative path for Addon to "../../adk/src/"
        addon_paths = ""
        paths = app_paths.split()
        for path in paths:
            if('addons' not in path):    # Don't include other addon paths
                addon_paths += ' %s' % path
        
        addon_paths = addon_paths.replace('../../../adk/', '../../../../adk/')
        addon_paths = addon_paths.replace('..\\..\\..\\adk\\', '../../../../adk/')
        
        app_src_dir = os.path.dirname(self.args.project).replace('\\','/') + '/../../src'
        addon_paths = addon_paths.replace('../../src', app_src_dir)
        addon_paths = addon_paths.replace('..\\..\\src', app_src_dir)
         
        self.addon_utils.patchFile(xml_config_fragment % (config_path_type, addon_paths), self.addon_x2p_file)

    def patchLoadedWorkspaceFile(self, addon_name, chip_type):
        f = open(self.app_templates_workspace_file,"r")
        workspace_patch = f.read()
        f.close()
        workspace_patch = workspace_patch.replace('{CHIP_TYPE}', chip_type)        
        self.addon_utils.patchFile(workspace_patch, self.args.workspace)    
 
    def patchLoadedAppProjectFile(self):
        self.addon_utils.executePatchScript(self.args.project, self.app_templates_project_file)

    def patchLoadedPromptsFile(self):
        self.addon_utils.executePatchScript(self.ro_fs_project_file, self.ro_fs_templates_project_file)

    def loadAddonIntoWorkspace(self, selected_addon_dir_path, addon_name):
        # Need to identify the correct project file by reading the chip type from the loaded application x2p
        chip_type = self.addon_utils.readAppProjectConfigItem("CHIP_TYPE", self.args.project)
        self.addon_x2p_file = os.path.join(selected_addon_dir_path, ('projects/%s/%s.x2p' % (chip_type, addon_name)))
                        
        self.patchAddonProjectFile("INCPATHS")
        self.patchAddonProjectFile("LIBPATHS")        
        self.patchLoadedWorkspaceFile(addon_name, chip_type)
        self.patchLoadedAppProjectFile()
    
        # Merge ro_fs project templates        
        if os.path.exists(self.ro_fs_templates_project_file):
            self.patchLoadedPromptsFile()

    def checkPrerequisites(self, selected_addon_dir_path):                    
        # Parent directory prerequisites
        self.checkPrerequisiteExists(os.path.join(selected_addon_dir_path, '../../addons'))
        
        # Root directory prerequisites
        self.checkPrerequisiteExists(os.path.join(selected_addon_dir_path, "projects"))
        self.checkPrerequisiteExists(os.path.join(selected_addon_dir_path, "src"))
        self.checkPrerequisiteExists(os.path.join(selected_addon_dir_path, "tools"))
        self.checkPrerequisiteExists(os.path.join(selected_addon_dir_path, "makefile"))
        
        # project directory prerequisites
        self.checkPrerequisiteExists(os.path.join(selected_addon_dir_path, 'projects/qcc512x_qcc302x/gaa.x2p'))
        self.checkPrerequisiteExists(os.path.join(selected_addon_dir_path, 'projects/qcc512x_rom_v21/gaa.x2p'))
        self.checkPrerequisiteExists(os.path.join(selected_addon_dir_path, 'projects/qcc514x_qcc304x/gaa.x2p'))
        
        # tools directory prerequisites                
        self.checkPrerequisiteExists(self.app_templates_project_file)
        self.checkPrerequisiteExists(self.app_templates_workspace_file)
        
        # adk tools prerequisites
        self.checkPrerequisiteExists(self.addon_utils_script)

    def createImportToolFilePaths(self, selected_addon_dir_path):
        self.addon_utils_script = os.path.join(selected_addon_dir_path + '/../../', self.addon_utils_script)
        self.app_templates_project_file = os.path.join(selected_addon_dir_path, self.app_templates_project_file)
        self.app_templates_workspace_file = os.path.join(selected_addon_dir_path, self.app_templates_workspace_file)
        self.ro_fs_templates_project_file = os.path.join(selected_addon_dir_path, self.ro_fs_templates_project_file)
        self.ro_fs_project_file = os.path.join(os.path.dirname(self.args.project), self.ro_fs_project_file)
    
    def importAddon(self):
        # Get project/workspace path so that we can define relative addresses
        project_path = os.path.dirname(self.args.project)
        addon_parent_dir = project_path + "/../../../"
        
        start_directory = os.path.join(addon_parent_dir, "addons")
        selected_addon_dir_path = self.ui.selectFolder(title='Select addon to import.', initialdir=start_directory)        
        
        if not selected_addon_dir_path:
            self.cancel()
        self.createImportToolFilePaths(selected_addon_dir_path)               
        self.checkPrerequisites(selected_addon_dir_path)
        addon_name = os.path.basename(selected_addon_dir_path)
        
        if self.ui.confirmAction(addon_name):            
            self.loadAddonIntoWorkspace(selected_addon_dir_path, addon_name)
            self.success()                
        else:
            self.cancel()
        
        
def parse_args():
    """ parse the command line arguments """
    parser = argparse.ArgumentParser(
                description='Import an Addon')

    parser.add_argument('-w', '--workspace',
                        required=True,
                        help='Specifies the workspace to use')

    parser.add_argument('-p', '--project',
                        required=True,
                        help='Specifies the project file to use')
                        
    parser.add_argument('-k', '--kit',
                        required=True,
                        help='Specifies the kit to use')

    return parser.parse_args()

if __name__ == '__main__':
    args = parse_args()
    Importer(args).importAddon()
