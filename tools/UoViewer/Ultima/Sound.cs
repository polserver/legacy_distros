using System;
using System.Collections.Generic;
using System.Text;
using System.Media;
using System.IO;
using System.Text.RegularExpressions;
using System.Runtime.InteropServices;

namespace Ultima
{
	public sealed class UOSound
	{
		public string Name;
		public MemoryStream WAVEStream;
        public int ID;

		public UOSound( string name, MemoryStream stream, int id )
		{
			Name = name;
			WAVEStream = stream;
            ID = id;
		}
	};

	public static class Sounds
	{
		private static BinaryReader m_Index;
		private static Stream m_Stream;
		private static Dictionary<int, int> m_Translations;
        private static Stream m_Indexstream;

		static Sounds()
		{
            m_Indexstream = new FileStream(Client.GetFilePath("soundidx.mul"), FileMode.Open,FileAccess.Read,FileShare.Read);
			m_Index = new BinaryReader( m_Indexstream );
			m_Stream = new FileStream( Client.GetFilePath( "sound.mul" ), FileMode.Open,FileAccess.Read,FileShare.Read );
			Regex reg = new Regex( @"(\d{1,3}) \x7B(\d{1,3})\x7D (\d{1,3})", RegexOptions.Compiled );

			m_Translations = new Dictionary<int, int>();

			string line;
			using( StreamReader reader = new StreamReader( Client.GetFilePath( "Sound.def" ) ) )
			{
				while( ( line = reader.ReadLine() ) != null )
				{
					if( ( ( line = line.Trim() ).Length != 0 ) && !line.StartsWith( "#" ) )
					{
						Match match = reg.Match( line );

						if( match.Success )
						{
							m_Translations.Add( int.Parse( match.Groups[ 1 ].Value ), int.Parse( match.Groups[ 2 ].Value ) );
						}
					}
				}
			}
		}

		public static UOSound GetSound( int soundID )
		{
			if( soundID < 0 ) { return null; }

			m_Index.BaseStream.Seek( (long)( soundID * 12 ), SeekOrigin.Begin );

			int offset = m_Index.ReadInt32();
			int length = m_Index.ReadInt32();
			int extra = m_Index.ReadInt32();

			if( ( offset < 0 ) || ( length <= 0 ) )
			{
				if( !m_Translations.TryGetValue( soundID, out soundID ) ) { return null; }

				m_Index.BaseStream.Seek( (long)( soundID * 12 ), SeekOrigin.Begin );

				offset = m_Index.ReadInt32();
				length = m_Index.ReadInt32();
				extra = m_Index.ReadInt32();
			}

			if( ( offset < 0 ) || ( length <= 0 ) ) { return null; }

			int[] waveHeader = WaveHeader( length );

			length -= 40;

			byte[] stringBuffer = new byte[ 40 ];
			byte[] buffer = new byte[ length ];

			m_Stream.Seek( (long)( offset ), SeekOrigin.Begin );
			m_Stream.Read( stringBuffer, 0, 40 );
			m_Stream.Read( buffer, 0, length );

			byte[] resultBuffer = new byte[ buffer.Length + ( waveHeader.Length << 2 ) ];

			Buffer.BlockCopy( waveHeader, 0, resultBuffer, 0, ( waveHeader.Length << 2 ) );
			Buffer.BlockCopy( buffer, 0, resultBuffer, ( waveHeader.Length << 2 ), buffer.Length );

			string str = System.Text.Encoding.ASCII.GetString( stringBuffer ); // seems that the null terminator's not being properly recognized :/
			return new UOSound( str.Substring( 0, str.IndexOf( '\0' ) ), new MemoryStream( resultBuffer ),soundID );
		}

		private static int[] WaveHeader( int length )
		{
			/* ====================
			 * = WAVE File layout =
			 * ====================
			 * char[4] = 'RIFF' \
			 * int - chunk size |- Riff Header
			 * char[4] = 'WAVE' /
			 * char[4] = 'fmt ' \
			 * int - chunk size |
			 * short - format	|
			 * short - channels	|
			 * int - samples p/s|- Format header
			 * int - avg bytes	|
			 * short - align	|
			 * short - bits p/s /
			 * char[4] - data	\
			 * int - chunk size | - Data header
			 * short[..] - data /
			 * ====================
			 * */
			return new int[] { 0x46464952, ( length + 12 ), 0x45564157, 0x20746D66, 0x10, 0x010001, 0x5622, 0xAC44, 0x100002, 0x61746164, ( length - 24 ) };
		}

        public static string IsValidSound(int soundID)
        {
            if( soundID < 0 ) { return ""; }

			m_Index.BaseStream.Seek( (long)( soundID * 12 ), SeekOrigin.Begin );

			int offset = m_Index.ReadInt32();
			int length = m_Index.ReadInt32();

			if( ( offset < 0 ) || ( length <= 0 ) )
			{
				if( !m_Translations.TryGetValue( soundID, out soundID ) ) { return ""; }

				m_Index.BaseStream.Seek( (long)( soundID * 12 ), SeekOrigin.Begin );

				offset = m_Index.ReadInt32();
				length = m_Index.ReadInt32();
			}

			if( ( offset < 0 ) || ( length <= 0 ) ) { return ""; }

            byte[] stringBuffer = new byte[40];
            m_Stream.Seek((long)(offset), SeekOrigin.Begin);
            m_Stream.Read(stringBuffer, 0, 40);
            string str = System.Text.Encoding.ASCII.GetString(stringBuffer); // seems that the null terminator's not being properly recognized :/
            str=str.Substring(0, str.IndexOf('\0'));
            return str;
        }

        public static int GetSoundLength(int soundID)
        {
            if (soundID < 0) { return 0; }

            m_Index.BaseStream.Seek((long)(soundID * 12), SeekOrigin.Begin);

            int offset = m_Index.ReadInt32();
            int length = m_Index.ReadInt32();

            if ((offset < 0) || (length <= 0))
            {
                if (!m_Translations.TryGetValue(soundID, out soundID)) { return 0; }

                m_Index.BaseStream.Seek((long)(soundID * 12), SeekOrigin.Begin);

                offset = m_Index.ReadInt32();
                length = m_Index.ReadInt32();
            }

            if ((offset < 0) || (length <= 0)) { return 0; }

            length -= 24;
            length /= 0xAC44;  // Bytes per Sec
            return length;
        }
	}
}
