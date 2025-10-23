<?xml version="1.0" encoding="UTF-8"?>
<!--
  ~                                  ***   StarForth   ***
  ~
  ~  custom-html.xsl- FORTH-79 Standard and ANSI C99 ONLY
  ~  Modified by - rajames
  ~  Last modified - 2025-10-23T10:55:24.596-04
  ~
  ~  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.
  ~
  ~  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  ~  To the extent possible under law, the author(s) have dedicated all copyright and related
  ~  and neighboring rights to this software to the public domain worldwide.
  ~  This software is distributed without any warranty.
  ~
  ~  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
  ~
  ~  /home/rajames/CLionProjects/StarForth/docs/xsl/custom-html.xsl
  -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

    <!-- Import stock DocBook HTML stylesheet (adjust path if needed) -->
    <xsl:import href="/usr/share/xml/docbook/stylesheet/docbook-xsl/html/docbook.xsl"/>

    <!-- Output settings -->
    <xsl:output method="html" encoding="UTF-8" indent="no"/>

    <!-- 1) Kill legacy body attributes (bgcolor/text/link/…): -->
    <xsl:template name="body.attributes"/>
    <!-- (By defining this empty, DocBook won't emit those attributes.) -->

    <!-- 2) Provide a default stylesheet param (you can also pass via -\-stringparam): -->
    <xsl:param name="html.stylesheet" select="'css/dark.css'"/>

    <!-- 3) Head hook: append UTF-8 meta, Bootstrap, etc. -->
    <xsl:template name="user.head.content">
        <!-- Ensure modern charset meta is present and late in head -->
        <meta charset="UTF-8"/>
        <!-- Your dark theme is added via html.stylesheet param; keep it. -->

        <!-- Optional: Bootstrap 5 (comment out if not wanted) -->
        <link rel="stylesheet"
              href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css"/>
        <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"
                defer="defer"></script>
    </xsl:template>

    <!-- 4) Footer hook: add your site JS at end of body -->
    <xsl:template name="user.footer.content">
        <script src="js/custom.js" defer="defer"></script>
    </xsl:template>

    <!-- 5) (Optional) Make <pre> blocks friendlier for dark background -->
    <xsl:template match="pre">
        <pre class="code-block">
            <xsl:apply-templates select="@*|node()"/>
        </pre>
    </xsl:template>

</xsl:stylesheet>
