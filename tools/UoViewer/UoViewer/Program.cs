/***************************************************************************
 *
 * $Author: Turley
 * 
 * "THE BEER-WARE LICENSE"
 * As long as you retain this notice you can do whatever you want with 
 * this stuff. If we meet some day, and you think this stuff is worth it,
 * you can buy me a beer in return.
 *
 ***************************************************************************/

using System;
using System.Windows.Forms;

namespace UoViewer
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>

        
        public static Options opt = new Options();
        [STAThread]
        static void Main()
        {
            try
            {
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new UoViewer());
                Options.Save();
            }
            catch (Exception err)
            {
                Clipboard.SetDataObject(err.ToString(), true);
                Application.Run(new ExceptionForm(err));
            }
        }
    }
}