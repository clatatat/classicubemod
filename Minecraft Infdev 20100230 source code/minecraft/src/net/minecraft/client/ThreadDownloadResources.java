package net.minecraft.client;

import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.ArrayList;

public final class ThreadDownloadResources extends Thread {
	private File resourcesFolder;
	private Minecraft mc;
	private boolean closing = false;

	public ThreadDownloadResources(File var1, Minecraft var2) {
		this.mc = var2;
		this.setName("Resource download thread");
		this.setDaemon(true);
		this.resourcesFolder = new File(var1, "resources/");
		if(!this.resourcesFolder.exists() && !this.resourcesFolder.mkdirs()) {
			throw new RuntimeException("The working directory could not be created: " + this.resourcesFolder);
		}
	}

	public final void run() {
				try {
			final ArrayList<String> list = new ArrayList<String>();
			final URL url = new URL("http://www.minecraft.net/resources/");
			final BufferedReader bufferedReader = new BufferedReader(new InputStreamReader(url.openStream()));
			String line;
			while ((line = bufferedReader.readLine()) != null) {
				list.add(line);
			}
			bufferedReader.close();
			for (int i = 0; i < list.size(); ++i) {
				final URL url2 = url;
				final String s = list.get(i);
				final URL context = url2;
				Label_0334: {
					try {
						final String[] split;
						final String child = (split = s.split(","))[0];
						final int int1 = Integer.parseInt(split[1]);
						Long.parseLong(split[2]);
						final File file;
						if (!(file = new File(this.resourcesFolder, child)).exists() || file.length() != int1) {
							file.getParentFile().mkdirs();
							this.downloadResource(new URL(context, child.replaceAll(" ", "%20")), file);
							if (this.closing) {
								break Label_0334;
							}
						}
						final Minecraft mc = this.mc;
						final String s2 = child;
						final File file2 = file;
						final String s3 = s2;
						final Minecraft minecraft = mc;
						final int index = s3.indexOf("/");
						final String substring = s3.substring(0, index);
						final String substring2 = s3.substring(index + 1);
						if (substring.equalsIgnoreCase("sound")) {
							minecraft.sndManager.addSound(substring2, file2);
						}
						else if (substring.equalsIgnoreCase("newsound")) {
							minecraft.sndManager.addSound(substring2, file2);
						}
						else if (substring.equalsIgnoreCase("music")) {
							minecraft.sndManager.addMusic(substring2, file2);
						}
					}
					catch (Exception ex) {
						ex.printStackTrace();
					}
				}
				if (this.closing) {
					return;
				}
			}
		}
		catch (IOException ex2) {
			ex2.printStackTrace();
		}
	}

	private void downloadResource(URL var1, File var2) throws IOException {
		byte[] var3 = new byte[4096];
		DataInputStream var5 = new DataInputStream(var1.openStream());
		DataOutputStream var6 = new DataOutputStream(new FileOutputStream(var2));

		do {
			int var4 = var5.read(var3);
			if(var4 < 0) {
				var5.close();
				var6.close();
				return;
			}

			var6.write(var3, 0, var4);
		} while(!this.closing);

	}

	public final void closeMinecraft() {
		this.closing = true;
	}
}
