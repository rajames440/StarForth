/*  Title:      Pure/Admin/component_eptcs.scala
    Author:     Makarius

Build Isabelle component for EPTCS LaTeX style.

See also:
  - http://style.eptcs.org
  - https://github.com/EPTCS/style/releases
*/

package isabelle


object Component_EPTCS {
  /* build eptcs component */

  val default_url = "https://github.com/EPTCS/style/releases/download"
  val default_version = "1.7.0"

  def build_eptcs(
    base_url: String = default_url,
    version: String = default_version,
    target_dir: Path = Path.current,
    progress: Progress = new Progress
  ): Unit = {
    /* component */

    val component = "eptcs-" + version
    val component_dir =
      Components.Directory(target_dir + Path.basic(component)).create(progress = progress)


    /* download */

    val download_url = base_url + "/v" + version + "/eptcsstyle.zip"

    Isabelle_System.with_tmp_file("download", ext = "zip") { download_file =>
      Isabelle_System.download_file(download_url, download_file, progress = progress)
      Isabelle_System.extract(download_file, component_dir.path)
    }


    /* settings */

    component_dir.write_settings("""
ISABELLE_EPTCS_HOME="$COMPONENT"
""")


    /* README */

    File.write(component_dir.README,
      """This is the EPTCS style from
""" + download_url + """


    Makarius
    """ + Date.Format.date(Date.now()) + "\n")
  }


  /* Isabelle tool wrapper */

  val isabelle_tool =
    Isabelle_Tool("component_eptcs", "build component for EPTCS LaTeX style",
      Scala_Project.here,
      { args =>
        var target_dir = Path.current
        var base_url = default_url
        var version = default_version

        val getopts = Getopts("""
Usage: isabelle component_eptcs [OPTIONS]

  Options are:
    -D DIR       target directory (default ".")
    -U URL       download URL (default: """" + default_url + """")
    -V VERSION   version (default: """" + default_version + """")

  Build component for EPTCS LaTeX style.
""",
          "D:" -> (arg => target_dir = Path.explode(arg)),
          "U:" -> (arg => base_url = arg),
          "V:" -> (arg => version = arg))

        val more_args = getopts(args)
        if (more_args.nonEmpty) getopts.usage()

        val progress = new Console_Progress()

        build_eptcs(base_url = base_url, version = version, target_dir = target_dir,
          progress = progress)
      })
}
