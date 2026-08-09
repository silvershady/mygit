import java.awt.*;
import java.awt.event.*;

public class AWTTest {
    public static void main(String[] args) {
        Frame frame = new Frame("Java AWT Window Test");
        Label label = new Label("Hello from Java AWT GUI!", Label.CENTER);
        frame.add(label);
        frame.setSize(400, 200);
        frame.setLayout(new FlowLayout());
        
        frame.addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent e) {
                frame.dispose();
                System.exit(0);
            }
        });
        
        frame.setVisible(true);
        System.out.println("AWT Frame displayed successfully!");
    }
}
