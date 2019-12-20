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

templates_dir = 'tools/templates/'
adk_tools_dir = 'adk/tools/'
app_templates_project_file = templates_dir + 'addon_ext_appsp1_x2p.xml'
app_templates_workspace_file = templates_dir + 'addon_ext_appsp1_x2w.xml'
ro_fs_templates_project_file = templates_dir + 'addon_ext_ro_fs_x2p.xml'
addon_utils_script = adk_tools_dir  + 'addon_utils/addon_utils.py'
ro_fs_project_file = 'filesystems/ro_fs.x2p'
appsp1_x2p_relative_paths2root = '../../../'
appsp1_x2p_relative_path2adk = appsp1_x2p_relative_paths2root + 'adk/'
appsp1_x2p_relative_path2appsrc = '../../src'
appsp1_x2p_relative_path2addon_utils = appsp1_x2p_relative_paths2root + adk_tools_dir + 'addon_utils'
addon_x2p_relative_path2adk = '../../../../adk/'

addon_select_info_message = 'About to import addon: {0}.\n\nThis operation will modify {1} and {2} files.\n\nProceed?'
addon_import_success_message = 'Addon {} operation successful'
addon_import_cancel_message = 'Addon {} operation cancelled'

addon_dir_prerequisites = ['../../addons', 'notices', 'projects', 'src', 'tools', 'tools/templates', app_templates_project_file, app_templates_workspace_file]

xml_config_fragment = """\
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
        if self.args.addon_path is None:
            selected_addon_dir_path = tkFileDialog.askdirectory(parent=self.root, **options)            
        else:
            selected_addon_dir_path = self.args.addon_path
        return selected_addon_dir_path

    def success(self, action):
        message = addon_import_success_message.format(action.lower())
        print(message)
        if self.args.addon_path is None:
            tkMessageBox.showinfo(message=message)

    def cancel(self, action):
        message = addon_import_cancel_message.format(action.lower())
        print(message)
        if self.args.addon_path is None:
            tkMessageBox.showinfo(message=message)
        sys.exit(0)

    def fail(self, msg):
        print(msg)
        if self.args.addon_path is None:
            tkMessageBox.showerror(message=msg)
        sys.exit(1)

    def confirmAction(self, addon_name):
        proceed = True
        info = addon_select_info_message.format(addon_name, self.args.project, self.args.workspace)
        print(info)
        if self.args.addon_path is None:
            proceed = tkMessageBox.askokcancel('Attention!', info)
        print(proceed)
        return proceed

class AddonUtils(object):
    def __init__(self, project_path):
        sys.path.append(os.path.join(os.path.dirname(project_path), appsp1_x2p_relative_path2addon_utils))
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
            print("Script Error")
        return result.getvalue()

    def executePatchScript(self, input_file, templates_file):
        self.util_script_execute(["--input", input_file, "--merge", templates_file, "--output", input_file])

    def patchFile(self, merge_buffer, output_file):
        temp_addon_templates_file = os.path.join(os.path.dirname(output_file), 'addon_ext_temp.xml')
        f = open(temp_addon_templates_file, "w+")
        f.write(merge_buffer)
        f.close()        
        self.executePatchScript(output_file, temp_addon_templates_file)
        os.remove(temp_addon_templates_file)

    def readAppProjectConfigItem(self, config_path_type, project_file):
        config_items = self.util_script_execute(["--input", project_file, "--configs", config_path_type])
        config_items = config_items.replace('-D', '')
        config_items = config_items.replace('\n', '')
        return config_items

class Importer(object):
    def __init__(self, args):
        self.args = args
        self.ui = UI(args)
        self.addon_utils = AddonUtils(self.args.project)

        # Set base paths (relativeness will be set after folder has been selected)
        self.app_templates_project_file = app_templates_project_file
        self.app_templates_workspace_file = app_templates_workspace_file
        self.ro_fs_templates_project_file = ro_fs_templates_project_file
        self.addon_utils_script = addon_utils_script
        self.ro_fs_project_file = ro_fs_project_file
        self.addon_x2p_file = ''
        self.addon_name = ''
        
    def cancel(self):
        self.ui.cancel('import')

    def success(self):
        self.ui.success('import')

    def checkPrerequisiteExists(self, prerequisite):
        if not os.path.exists(prerequisite):
            self.ui.fail('no ' + prerequisite + ' found')
    
    def modifyRelativePaths(self, include_paths, old_rel_path_fragment, new_rel_path_fragment):
        include_paths = include_paths.replace(old_rel_path_fragment, new_rel_path_fragment)
        old_rel_path_fragment = old_rel_path_fragment.replace('/', '\\')
        include_paths = include_paths.replace(old_rel_path_fragment, new_rel_path_fragment)
        return include_paths
        
    def patchAddonProjectFile(self, config_path_type):
        app_paths = self.addon_utils.readAppProjectConfigItem(config_path_type, self.args.project)

        # Get all include paths from the application x2p, excluding other addon paths
        addon_paths = ""
        paths = app_paths.split()
        for path in paths:
            if('addons' not in path):
                addon_paths += ' %s' % path

        # Modify adk paths relative to addon x2p
        addon_paths = self.modifyRelativePaths(addon_paths, appsp1_x2p_relative_path2adk, addon_x2p_relative_path2adk)

        # Modify application src paths relative to addon x2p
        addon_x2p_relative_path2appsrc = os.path.join(os.path.dirname(self.args.project), appsp1_x2p_relative_path2appsrc)
        addon_paths = self.modifyRelativePaths(addon_paths, appsp1_x2p_relative_path2appsrc, addon_x2p_relative_path2appsrc).replace('\\', '/')
        self.addon_utils.patchFile(xml_config_fragment % (config_path_type, addon_paths), self.addon_x2p_file)

    def patchLoadedWorkspaceFile(self, chip_type):
        f = open(self.app_templates_workspace_file,"r")
        workspace_patch = f.read()
        f.close()
        workspace_patch = workspace_patch.replace('{CHIP_TYPE}', chip_type)        
        self.addon_utils.patchFile(workspace_patch, self.args.workspace)    
 
    def patchLoadedAppProjectFile(self):
        self.addon_utils.executePatchScript(self.args.project, self.app_templates_project_file)

    def patchLoadedPromptsFile(self):
        self.addon_utils.executePatchScript(self.ro_fs_project_file, self.ro_fs_templates_project_file)

    def createAddonProjectFile(self, selected_addon_dir_path, chip_type):
        # Create addon project file
        addon_x2p_template_file_path = os.path.join(selected_addon_dir_path, templates_dir, self.addon_name + '.x2p')
        addon_x2p_chip_specific_file_path = os.path.join(selected_addon_dir_path, 'projects', chip_type, self.addon_name + '.x2p')
        addon_x2p_chip_specific_dir = os.path.dirname(addon_x2p_chip_specific_file_path)
        if not os.path.exists(addon_x2p_chip_specific_dir):
            os.mkdir(addon_x2p_chip_specific_dir)
        copyfile(addon_x2p_template_file_path, addon_x2p_chip_specific_file_path)
        self.addon_x2p_file = os.path.join(selected_addon_dir_path, ('projects/%s/%s.x2p' % (chip_type, self.addon_name)))
        
    def loadAddonIntoWorkspace(self, selected_addon_dir_path):
        # Need to identify the correct project file by reading the chip type from the loaded application x2p
        chip_type = self.addon_utils.readAppProjectConfigItem("CHIP_TYPE", self.args.project)
        
        # Create addon project file
        self.createAddonProjectFile(selected_addon_dir_path, chip_type)
                        
        self.patchAddonProjectFile("INCPATHS")
        self.patchAddonProjectFile("LIBPATHS")
        self.patchAddonProjectFile("CHIP_TYPE")
        self.patchLoadedWorkspaceFile(chip_type)
        self.patchLoadedAppProjectFile()
    
        # Merge ro_fs project templates        
        if os.path.exists(self.ro_fs_templates_project_file):
            self.patchLoadedPromptsFile()

    def checkPrerequisites(self, selected_addon_dir_path):                          
        # Addon directory prerequisites
        for prereq_dir in addon_dir_prerequisites:
            self.checkPrerequisiteExists(os.path.join(selected_addon_dir_path, prereq_dir))
        
        # adk tools directory prerequisites
        self.checkPrerequisiteExists(self.addon_utils_script)

    def createImportToolFilePaths(self, selected_addon_dir_path):
        self.addon_utils_script = os.path.join(selected_addon_dir_path, '../..', self.addon_utils_script)
        self.app_templates_project_file = os.path.join(selected_addon_dir_path, self.app_templates_project_file)
        self.app_templates_workspace_file = os.path.join(selected_addon_dir_path, self.app_templates_workspace_file)
        self.ro_fs_templates_project_file = os.path.join(selected_addon_dir_path, self.ro_fs_templates_project_file)
        self.ro_fs_project_file = os.path.join(os.path.dirname(self.args.project), self.ro_fs_project_file)
    
    def importAddon(self):
        # Get project/workspace path so that we can define relative addresses
        project_path = os.path.dirname(self.args.project)
        addon_parent_dir = os.path.join(project_path, appsp1_x2p_relative_paths2root)
        
        start_directory = os.path.join(addon_parent_dir, "addons")
        selected_addon_dir_path = self.ui.selectFolder(title='Select addon to import.', initialdir=start_directory).replace('\\', '/')     
        
        if not selected_addon_dir_path:
            self.cancel()
        self.createImportToolFilePaths(selected_addon_dir_path)               
        self.addon_name = os.path.basename(selected_addon_dir_path)
        self.checkPrerequisites(selected_addon_dir_path)
        
        if self.ui.confirmAction(self.addon_name):            
            self.loadAddonIntoWorkspace(selected_addon_dir_path)
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
                        
    parser.add_argument('-ap', '--addon_path',
                        required=False,
                        help='Specifies the addon path (for non-GUI driven CI)')

    return parser.parse_args()

if __name__ == '__main__':
    args = parse_args()
    Importer(args).importAddon()
