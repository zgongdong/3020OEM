############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#
############################################################################
"""
Display tables in the console, whether it's a help message or otherwise
"""
from __future__ import print_function


class HelpSection(list):
    """
    Create and manage help sections

    Assumes each section in a help document consists of title, description
    and items to explain. Items are usually a pair of (command, description).
    Apart from all these necessary properties, it is possible to define some
    styles when the class is instantiated.

    Help section is basically a list and all the default operations in the
    list object typle is available for HelpSection.

    @param  width   The width of help section. This the number of characters
                    and the type is integer.
    @param  left_padding  The padding from the left side of the screen.
    @param  command_width The width of the command text in items.
    """

    def __init__(self, width=80, left_padding=8, command_width=20):
        super(HelpSection, self).__init__()

        self._title = list()
        self._description = list()

        self._width = width
        self._left_padding = left_padding
        self._first_item_width = command_width

        self._items = list()

    @property
    def title(self):
        """
        Title of the section

        @return A string
        """
        return ' '.join(self._title)

    @title.setter
    def title(self, value):
        """
        Set the title of the section

        It wraps the given title with the total width of the help section
        and then adds a blank line at the end of the title.

        @param value The title in string
        """
        if len(value):
            self._title = self._wrap_line(value, self._width)

            # Add a blank line
            self._title.append('')

    @property
    def description(self):
        """
        The description of the help section
        @return A string
        """
        return ' '.join(self._description)

    @description.setter
    def description(self, value):
        """
        Sets the description of the help section

        @param value The description in string
        """
        if len(value):
            self._description = value
            self._description = self._wrap_line(value, self._width)

            # Add a blank line
            self._description.append('')

    def add_item(self, command, description):
        """
        Add an help item to the section

        Help Item is the "command" "description" pair. This should be displayed
        something similar to:

        search:              Searches all the registers and variables

        @param  command         The command in integer
        @param  description     The description of the given command
        """
        self.append((command, description))

    def get_help(self, verbose=False):
        """
        Get the help as a list of strings or prints it

        @param  verbose When True, instead of returning a list of lines, the
                        method will print the results line by line.
        @return A list of strings
        """
        output = list()
        output.extend(self._title)
        output.extend(self._description)

        for first, second in sorted(self):
            output.extend(self._get_item_lines(first, second))

        if verbose:
            for item in output:
                print(item)
        else:
            return output

    def _get_item_lines(self, command, description):
        description_length = self._width - self._first_item_width
        item_format = '{0:>%d}{1:<%d}{2:<%d}' % (
            self._left_padding,
            self._first_item_width,
            description_length
        )

        if len(description) > description_length:
            description_lines = self._wrap_line(description, description_length)
        else:
            description_lines = [description]

        lines = [
            item_format.format(
                '',
                command,
                description_lines.pop(0),
            )
        ]

        # Generate the rest of the description
        while len(description_lines) > 0:
            lines.append(
                item_format.format(
                    '',
                    '',
                    description_lines.pop(0)
                )
            )

        return lines

    @staticmethod
    def _wrap_line(line, length):
        lines = []
        line_words = []
        for word in line.split():
            if (len(' '.join(line_words)) + len(word)) > length:
                lines.append(' '.join(line_words))
                line_words = [word]

            else:
                line_words.append(word)

        else:
            lines.append(' '.join(line_words))

        return lines
